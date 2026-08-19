package ai.mezon.mezia

import android.view.Surface

object Platform {
    init {
        System.loadLibrary("mezia_jni")
    }

    @JvmStatic
    external fun nativeCreate(
        localIp: String,
        localPort: Int,
        remoteIp: String,
        remotePort: Int,
        audioSsrc: Int,
        videoSsrc: Int,
        enableVideo: Boolean
    ): Long

    @JvmStatic external fun nativeStart(handle: Long): Int
    @JvmStatic external fun nativeStop(handle: Long): Int
    @JvmStatic external fun nativeDestroy(handle: Long)
    @JvmStatic external fun nativeSetWindow(handle: Long, surface: Surface?)
    @JvmStatic external fun nativeSetCamera(handle: Long, facing: Int): Int
    @JvmStatic external fun nativeStats(handle: Long): String
}
