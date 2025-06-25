package com.example.secondscreen

import android.app.AlertDialog
import android.content.Intent
import android.os.Build
import android.os.Bundle
import android.view.*
import androidx.appcompat.app.AppCompatActivity
import androidx.core.view.WindowCompat
import androidx.core.view.WindowInsetsCompat
import androidx.core.view.WindowInsetsControllerCompat

class MainActivity : AppCompatActivity() {
    private lateinit var surfaceView: SurfaceView

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        // Hide system bars for full immersion
        hideSystemUI()

        // Initialize surface view
        surfaceView = findViewById(R.id.videoSurface)
        surfaceView.holder.addCallback(object : SurfaceHolder.Callback {
            override fun surfaceCreated(holder: SurfaceHolder) {
                // Notify service that surface is ready
                val intent = Intent(this@MainActivity, VideoReceiverService::class.java)
                intent.action = VideoReceiverService.ACTION_SURFACE_CREATED
                intent.putExtra(VideoReceiverService.EXTRA_SURFACE, holder.surface)
                startService(intent)
            }

            override fun surfaceChanged(holder: SurfaceHolder, format: Int, width: Int, height: Int) {
                // Nothing needed here
            }

            override fun surfaceDestroyed(holder: SurfaceHolder) {
                // Notify service that surface is gone
                val intent = Intent(this@MainActivity, VideoReceiverService::class.java)
                intent.action = VideoReceiverService.ACTION_SURFACE_DESTROYED
                startService(intent)
            }
        })

        // Forward touch events to the background service
        setupTouchListener()

        // If first run, show permission dialog
        if (!getPreferences(MODE_PRIVATE).getBoolean(KEY_FIRST_RUN_COMPLETED, false)) {
            showPermissionDialog()
        } else {
            startVideoReceiverService()
        }
    }

    override fun onDestroy() {
        super.onDestroy()
        // Don't stop service here, since we want it to keep running in the background
    }

    private fun hideSystemUI() {
        WindowCompat.setDecorFitsSystemWindows(window, false)
        WindowInsetsControllerCompat(window, window.decorView).let { controller ->
            controller.hide(WindowInsetsCompat.Type.systemBars())
            controller.systemBarsBehavior =
                WindowInsetsControllerCompat.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE
        }
    }

    private fun showPermissionDialog() {
        AlertDialog.Builder(this)
            .setTitle(R.string.permission_dialog_title)
            .setMessage(R.string.permission_dialog_message)
            .setPositiveButton(R.string.permission_dialog_ok) { _, _ ->
                // Mark first run as completed
                getPreferences(MODE_PRIVATE).edit()
                    .putBoolean(KEY_FIRST_RUN_COMPLETED, true)
                    .apply()

                // Start the service
                startVideoReceiverService()
            }
            .setNegativeButton(R.string.permission_dialog_cancel) { _, _ ->
                finish()
            }
            .setCancelable(false)
            .show()
    }

    private fun startVideoReceiverService() {
        val intent = Intent(this, VideoReceiverService::class.java)
        intent.action = VideoReceiverService.ACTION_START

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            startForegroundService(intent)
        } else {
            startService(intent)
        }
    }

    private fun setupTouchListener() {
        surfaceView.setOnTouchListener { _, event ->
            val action = when (event.action and MotionEvent.ACTION_MASK) {
                MotionEvent.ACTION_DOWN -> "down"
                MotionEvent.ACTION_MOVE -> "move"
                MotionEvent.ACTION_UP,
                MotionEvent.ACTION_CANCEL,
                MotionEvent.ACTION_POINTER_UP -> "up"
                else -> return@setOnTouchListener true
            }

            // Process the touch event
            val pointerId = event.getPointerId(event.actionIndex)
            val normalizedX = event.x / surfaceView.width
            val normalizedY = event.y / surfaceView.height

            // Send touch event through service
            val intent = Intent(this, VideoReceiverService::class.java)
            intent.action = VideoReceiverService.ACTION_TOUCH_EVENT
            intent.putExtra("pointerId", pointerId)
            intent.putExtra("normalizedX", normalizedX)
            intent.putExtra("normalizedY", normalizedY)
            intent.putExtra("action", action)
            startService(intent)

            true
        }
    }

    companion object {
        private const val KEY_FIRST_RUN_COMPLETED = "first_run_completed"
    }
}
