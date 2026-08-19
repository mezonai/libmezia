package ai.mezon.mezia.demo

import android.Manifest
import android.content.pm.PackageManager
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.view.SurfaceHolder
import android.view.SurfaceView
import android.widget.Button
import android.widget.CheckBox
import android.widget.EditText
import android.widget.TextView
import android.widget.Toast
import androidx.appcompat.app.AppCompatActivity
import androidx.core.app.ActivityCompat
import androidx.core.content.ContextCompat
import ai.mezon.mezia.Platform
import kotlin.random.Random

class MainActivity : AppCompatActivity(), SurfaceHolder.Callback {
    private val permissions = arrayOf(
        Manifest.permission.RECORD_AUDIO,
        Manifest.permission.CAMERA
    )

    private lateinit var remoteSurface: SurfaceView
    private lateinit var remoteIp: EditText
    private lateinit var localPort: EditText
    private lateinit var remotePort: EditText
    private lateinit var enableVideo: CheckBox
    private lateinit var startStop: Button
    private lateinit var flipCamera: Button
    private lateinit var status: TextView

    private val handler = Handler(Looper.getMainLooper())
    private var handle = 0L
    private var running = false
    private var backCamera = false
    private var surfaceReady = false

    private val statsTick = object : Runnable {
        override fun run() {
            if (handle != 0L && running) {
                status.text = Platform.nativeStats(handle)
                handler.postDelayed(this, 1000)
            }
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        remoteSurface = findViewById(R.id.remoteSurface)
        remoteIp = findViewById(R.id.remoteIp)
        localPort = findViewById(R.id.localPort)
        remotePort = findViewById(R.id.remotePort)
        enableVideo = findViewById(R.id.enableVideo)
        startStop = findViewById(R.id.startStop)
        flipCamera = findViewById(R.id.flipCamera)
        status = findViewById(R.id.status)

        remoteSurface.holder.addCallback(this)
        startStop.setOnClickListener { toggle() }
        flipCamera.setOnClickListener {
            if (handle != 0L) {
                backCamera = !backCamera
                Platform.nativeSetCamera(handle, if (backCamera) 1 else 0)
            }
        }
        requestPerms()
    }

    private fun requestPerms() {
        val missing = permissions.filter {
            ContextCompat.checkSelfPermission(this, it) != PackageManager.PERMISSION_GRANTED
        }
        if (missing.isNotEmpty()) {
            ActivityCompat.requestPermissions(this, missing.toTypedArray(), 1)
        }
    }

    private fun hasPerms(): Boolean =
        permissions.all {
            ContextCompat.checkSelfPermission(this, it) == PackageManager.PERMISSION_GRANTED
        }

    private fun toggle() {
        if (running) {
            stopCall()
            return
        }
        if (!hasPerms()) {
            requestPerms()
            Toast.makeText(this, "Need microphone and camera", Toast.LENGTH_SHORT).show()
            return
        }
        if (!surfaceReady) {
            Toast.makeText(this, "Remote surface not ready yet", Toast.LENGTH_SHORT).show()
            return
        }
        val ip = remoteIp.text.toString().trim()
        val lp = localPort.text.toString().toIntOrNull() ?: 5004
        val rp = remotePort.text.toString().toIntOrNull() ?: 5004
        if (ip.isEmpty()) {
            Toast.makeText(this, "Set remote IP", Toast.LENGTH_SHORT).show()
            return
        }
        handle = Platform.nativeCreate(
            "0.0.0.0",
            lp,
            ip,
            rp,
            Random.nextInt(),
            Random.nextInt(),
            enableVideo.isChecked
        )
        if (handle == 0L) {
            status.text = "nativeCreate failed (check IP/port)"
            return
        }
        if (surfaceReady) {
            Platform.nativeSetWindow(handle, remoteSurface.holder.surface)
        }
        val rc = Platform.nativeStart(handle)
        if (rc != 0) {
            status.text = "nativeStart failed: $rc"
            Platform.nativeDestroy(handle)
            handle = 0L
            return
        }
        running = true
        startStop.text = "Stop"
        status.text = "Running UDP $lp -> $ip:$rp"
        handler.post(statsTick)
    }

    private fun stopCall() {
        handler.removeCallbacks(statsTick)
        running = false
        startStop.text = "Start"
        if (handle != 0L) {
            Platform.nativeStop(handle)
            Platform.nativeDestroy(handle)
            handle = 0L
        }
        status.text = "Stopped"
    }

    override fun surfaceCreated(holder: SurfaceHolder) {
        surfaceReady = true
        if (handle != 0L) {
            Platform.nativeSetWindow(handle, holder.surface)
        }
    }

    override fun surfaceChanged(holder: SurfaceHolder, format: Int, width: Int, height: Int) {
        if (handle != 0L) {
            Platform.nativeSetWindow(handle, holder.surface)
        }
    }

    override fun surfaceDestroyed(holder: SurfaceHolder) {
        surfaceReady = false
        if (handle != 0L) {
            Platform.nativeSetWindow(handle, null)
        }
    }

    override fun onDestroy() {
        stopCall()
        super.onDestroy()
    }
}
