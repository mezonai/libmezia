#include "mezia/peer.h"
#include "mezia_internal.h"

#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <windows.h>
#include <ws2tcpip.h>
typedef SOCKET mezon_socket_t;
#define MEZON_INVALID_SOCKET INVALID_SOCKET
#define mezon_close_socket closesocket
#else
#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
typedef int mezon_socket_t;
#define MEZON_INVALID_SOCKET (-1)
#define mezon_close_socket close
#endif

struct mezon_peer {
  mezon_peer_config_t config;
  char local_ip[64];
  char remote_ip[64];
  mezon_socket_t socket_fd;
  struct sockaddr_storage remote_addr;
  socklen_t remote_addr_len;
  uint16_t local_port;
  atomic_int running;
  mezon_stats_t stats;
#ifdef _WIN32
  HANDLE thread;
  CRITICAL_SECTION lock;
#else
  pthread_t thread;
  pthread_mutex_t lock;
#endif
};

static void report_error(mezon_peer_t *peer, mezon_status_t status) {
  if (peer->config.on_error) {
    peer->config.on_error(status, peer->config.user_data);
  }
}

static void lock_peer(mezon_peer_t *peer) {
#ifdef _WIN32
  EnterCriticalSection(&peer->lock);
#else
  pthread_mutex_lock(&peer->lock);
#endif
}

static void unlock_peer(mezon_peer_t *peer) {
#ifdef _WIN32
  LeaveCriticalSection(&peer->lock);
#else
  pthread_mutex_unlock(&peer->lock);
#endif
}

static int resolve_address(const char *host, uint16_t port, int passive,
                           struct sockaddr_storage *address,
                           socklen_t *address_len) {
  struct addrinfo hints;
  struct addrinfo *result = NULL;
  char service[6];
  int rc;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_DGRAM;
  hints.ai_protocol = IPPROTO_UDP;
  hints.ai_flags = passive ? AI_PASSIVE : 0;
  snprintf(service, sizeof(service), "%u", (unsigned)port);
  rc = getaddrinfo(host && host[0] ? host : NULL, service, &hints, &result);
  if (rc != 0 || !result || result->ai_addrlen > sizeof(*address)) {
    if (result) {
      freeaddrinfo(result);
    }
    return 0;
  }
  memcpy(address, result->ai_addr, result->ai_addrlen);
  *address_len = (socklen_t)result->ai_addrlen;
  freeaddrinfo(result);
  return 1;
}

static int same_endpoint(const struct sockaddr_storage *expected,
                         const struct sockaddr_storage *actual) {
  if (expected->ss_family != actual->ss_family) {
    return 0;
  }
  if (expected->ss_family == AF_INET) {
    const struct sockaddr_in *a = (const struct sockaddr_in *)expected;
    const struct sockaddr_in *b = (const struct sockaddr_in *)actual;
    return a->sin_port == b->sin_port &&
           a->sin_addr.s_addr == b->sin_addr.s_addr;
  }
  if (expected->ss_family == AF_INET6) {
    const struct sockaddr_in6 *a = (const struct sockaddr_in6 *)expected;
    const struct sockaddr_in6 *b = (const struct sockaddr_in6 *)actual;
    return a->sin6_port == b->sin6_port &&
           memcmp(&a->sin6_addr, &b->sin6_addr, sizeof(a->sin6_addr)) == 0;
  }
  return 0;
}

static void receive_loop(mezon_peer_t *peer) {
  uint8_t datagram[MEZON_MAX_DATAGRAM_SIZE];
  while (atomic_load(&peer->running)) {
    struct sockaddr_storage source;
    socklen_t source_len = sizeof(source);
#ifdef _WIN32
    int received = recvfrom(peer->socket_fd, (char *)datagram, sizeof(datagram),
                            0, (struct sockaddr *)&source, &source_len);
    if (received == SOCKET_ERROR) {
      int error = WSAGetLastError();
      if (!atomic_load(&peer->running)) {
        break;
      }
      if (error == WSAETIMEDOUT || error == WSAEWOULDBLOCK) {
        continue;
      }
#else
    ssize_t received = recvfrom(peer->socket_fd, datagram, sizeof(datagram), 0,
                                (struct sockaddr *)&source, &source_len);
    if (received < 0) {
      if (!atomic_load(&peer->running)) {
        break;
      }
      if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
        continue;
      }
#endif
      lock_peer(peer);
      peer->stats.socket_errors++;
      unlock_peer(peer);
      report_error(peer, MEZON_ERR_NETWORK);
      continue;
    }
    if (!same_endpoint(&peer->remote_addr, &source)) {
      continue;
    }
    {
      mezon_packet_t packet;
      mezon_status_t status =
          mezon_rtp_parse(datagram, (size_t)received, &packet);
      if (status != MEZON_OK) {
        lock_peer(peer);
        peer->stats.malformed_packets++;
        unlock_peer(peer);
        continue;
      }
      packet.arrival_time_ns = mezon_clock_now_ns();
      lock_peer(peer);
      peer->stats.packets_received++;
      peer->stats.bytes_received += (uint64_t)received;
      unlock_peer(peer);
      if (peer->config.on_packet) {
        peer->config.on_packet(&packet, peer->config.user_data);
      }
    }
  }
}

#ifdef _WIN32
static DWORD WINAPI receive_thread(LPVOID value) {
  receive_loop((mezon_peer_t *)value);
  return 0;
}
#else
static void *receive_thread(void *value) {
  receive_loop((mezon_peer_t *)value);
  return NULL;
}
#endif

mezon_peer_t *mezon_peer_create(const mezon_peer_config_t *config) {
  mezon_peer_t *peer;
  if (!config || !config->remote_ip || !config->remote_ip[0] ||
      config->remote_port == 0 || config->mtu > MEZON_MAX_DATAGRAM_SIZE ||
      (config->mtu && config->mtu <= MEZON_RTP_HEADER_SIZE)) {
    return NULL;
  }
  peer = (mezon_peer_t *)calloc(1, sizeof(*peer));
  if (!peer) {
    return NULL;
  }
  peer->config = *config;
  peer->config.mtu = config->mtu ? config->mtu : MEZON_DEFAULT_MTU;
  snprintf(peer->local_ip, sizeof(peer->local_ip), "%s",
           config->local_ip ? config->local_ip : "0.0.0.0");
  snprintf(peer->remote_ip, sizeof(peer->remote_ip), "%s", config->remote_ip);
  peer->config.local_ip = peer->local_ip;
  peer->config.remote_ip = peer->remote_ip;
  peer->socket_fd = MEZON_INVALID_SOCKET;
#ifdef _WIN32
  InitializeCriticalSection(&peer->lock);
#else
  pthread_mutex_init(&peer->lock, NULL);
#endif
  return peer;
}

void mezon_peer_destroy(mezon_peer_t *peer) {
  if (!peer) {
    return;
  }
  mezon_peer_stop(peer);
#ifdef _WIN32
  DeleteCriticalSection(&peer->lock);
#else
  pthread_mutex_destroy(&peer->lock);
#endif
  free(peer);
}

mezon_status_t mezon_peer_start(mezon_peer_t *peer) {
  struct sockaddr_storage local_addr;
  socklen_t local_len;
#ifndef _WIN32
  struct timeval timeout;
#endif
  int reuse = 1;
  if (!peer) {
    return MEZON_ERR_INVALID_ARG;
  }
  if (atomic_load(&peer->running)) {
    return MEZON_OK;
  }
#ifdef _WIN32
  {
    WSADATA data;
    if (WSAStartup(MAKEWORD(2, 2), &data) != 0) {
      return MEZON_ERR_NETWORK;
    }
  }
#endif
  if (!resolve_address(peer->remote_ip, peer->config.remote_port, 0,
                       &peer->remote_addr, &peer->remote_addr_len) ||
      !resolve_address(peer->local_ip, peer->config.local_port, 1, &local_addr,
                       &local_len)) {
    return MEZON_ERR_INVALID_ARG;
  }
  if (local_addr.ss_family != peer->remote_addr.ss_family) {
    return MEZON_ERR_INVALID_ARG;
  }
  peer->socket_fd = socket(local_addr.ss_family, SOCK_DGRAM, IPPROTO_UDP);
  if (peer->socket_fd == MEZON_INVALID_SOCKET) {
    return MEZON_ERR_NETWORK;
  }
  setsockopt(peer->socket_fd, SOL_SOCKET, SO_REUSEADDR, (const char *)&reuse,
             sizeof(reuse));
#ifdef _WIN32
  {
    DWORD timeout_ms = 100;
    setsockopt(peer->socket_fd, SOL_SOCKET, SO_RCVTIMEO,
               (const char *)&timeout_ms, sizeof(timeout_ms));
  }
#else
  timeout.tv_sec = 0;
  timeout.tv_usec = 100000;
  setsockopt(peer->socket_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout,
             sizeof(timeout));
#endif
  if (peer->config.receive_buffer_bytes > 0) {
    setsockopt(peer->socket_fd, SOL_SOCKET, SO_RCVBUF,
               (const char *)&peer->config.receive_buffer_bytes, sizeof(int));
  }
  if (peer->config.send_buffer_bytes > 0) {
    setsockopt(peer->socket_fd, SOL_SOCKET, SO_SNDBUF,
               (const char *)&peer->config.send_buffer_bytes, sizeof(int));
  }
  if (bind(peer->socket_fd, (struct sockaddr *)&local_addr, local_len) != 0) {
    mezon_close_socket(peer->socket_fd);
    peer->socket_fd = MEZON_INVALID_SOCKET;
    return MEZON_ERR_NETWORK;
  }
  {
    struct sockaddr_storage bound;
    socklen_t bound_len = sizeof(bound);
    if (getsockname(peer->socket_fd, (struct sockaddr *)&bound, &bound_len) ==
        0) {
      peer->local_port = bound.ss_family == AF_INET
                             ? ntohs(((struct sockaddr_in *)&bound)->sin_port)
                             : ntohs(((struct sockaddr_in6 *)&bound)->sin6_port);
    }
  }
  atomic_store(&peer->running, 1);
#ifdef _WIN32
  peer->thread = CreateThread(NULL, 0, receive_thread, peer, 0, NULL);
  if (!peer->thread) {
#else
  if (pthread_create(&peer->thread, NULL, receive_thread, peer) != 0) {
#endif
    atomic_store(&peer->running, 0);
    mezon_close_socket(peer->socket_fd);
    peer->socket_fd = MEZON_INVALID_SOCKET;
    return MEZON_ERR_INTERNAL;
  }
  return MEZON_OK;
}

mezon_status_t mezon_peer_stop(mezon_peer_t *peer) {
  if (!peer) {
    return MEZON_ERR_INVALID_ARG;
  }
  if (!atomic_load(&peer->running)) {
    return MEZON_OK;
  }
#ifdef _WIN32
  if (GetCurrentThreadId() == GetThreadId(peer->thread)) {
    return MEZON_ERR_STATE;
  }
#else
  if (pthread_equal(pthread_self(), peer->thread)) {
    return MEZON_ERR_STATE;
  }
#endif
  atomic_store(&peer->running, 0);
#ifdef _WIN32
  shutdown(peer->socket_fd, SD_BOTH);
  WaitForSingleObject(peer->thread, INFINITE);
  CloseHandle(peer->thread);
#else
  shutdown(peer->socket_fd, SHUT_RDWR);
  pthread_join(peer->thread, NULL);
#endif
  mezon_close_socket(peer->socket_fd);
  peer->socket_fd = MEZON_INVALID_SOCKET;
  return MEZON_OK;
}

mezon_status_t mezon_peer_send(mezon_peer_t *peer,
                               const mezon_packet_t *packet) {
  uint8_t datagram[MEZON_MAX_DATAGRAM_SIZE];
  size_t datagram_len = 0;
  mezon_status_t status;
  if (!peer || !packet) {
    return MEZON_ERR_INVALID_ARG;
  }
  if (!atomic_load(&peer->running)) {
    return MEZON_ERR_NOT_READY;
  }
  status = mezon_rtp_serialize(packet, datagram, peer->config.mtu,
                               &datagram_len);
  if (status != MEZON_OK) {
    return status;
  }
#ifdef _WIN32
  if (sendto(peer->socket_fd, (const char *)datagram, (int)datagram_len, 0,
             (struct sockaddr *)&peer->remote_addr,
             peer->remote_addr_len) == SOCKET_ERROR) {
    int error = WSAGetLastError();
    return error == WSAEWOULDBLOCK ? MEZON_ERR_WOULD_BLOCK
                                   : MEZON_ERR_NETWORK;
  }
#else
  if (sendto(peer->socket_fd, datagram, datagram_len, 0,
             (struct sockaddr *)&peer->remote_addr,
             peer->remote_addr_len) < 0) {
    return errno == EAGAIN || errno == EWOULDBLOCK ? MEZON_ERR_WOULD_BLOCK
                                                   : MEZON_ERR_NETWORK;
  }
#endif
  lock_peer(peer);
  peer->stats.packets_sent++;
  peer->stats.bytes_sent += datagram_len;
  unlock_peer(peer);
  return MEZON_OK;
}

uint16_t mezon_peer_local_port(const mezon_peer_t *peer) {
  return peer ? peer->local_port : 0;
}

void mezon_peer_get_stats(const mezon_peer_t *peer, mezon_stats_t *stats) {
  mezon_peer_t *mutable_peer = (mezon_peer_t *)peer;
  if (!peer || !stats) {
    return;
  }
  lock_peer(mutable_peer);
  *stats = peer->stats;
  unlock_peer(mutable_peer);
}
