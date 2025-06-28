package com.example.secondscreen

import android.app.AlertDialog
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.os.Build
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.util.Log
import android.view.*
import android.widget.TextView
import androidx.appcompat.app.AppCompatActivity
import androidx.core.view.WindowCompat
import androidx.core.view.WindowInsetsCompat
import androidx.core.view.WindowInsetsControllerCompat

class MainActivity : AppCompatActivity() {
    private lateinit var surfaceView: SurfaceView
    private lateinit var statusText: TextView
    private lateinit var debugText: TextView
    private var debugReceiver: BroadcastReceiver? = null
    private val debugHandler = Handler(Looper.getMainLooper())
    private val debugLines = mutableListOf<String>()
    private val maxDebugLines = 10

    companion object {
        private const val TAG = "TabDisplay_MainActivity"
        private const val KEY_FIRST_RUN_COMPLETED = "first_run_completed"
        
        // Debug broadcast actions
        const val ACTION_DEBUG_UPDATE = "com.example.secondscreen.DEBUG_UPDATE"
        const val EXTRA_DEBUG_MESSAGE = "debug_message"
        const val EXTRA_DEBUG_LEVEL = "debug_level"
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        Log.d(TAG, "MainActivity onCreate started")

        setContentView(R.layout.activity_main)

        // Initialize UI elements
        surfaceView = findViewById(R.id.videoSurface)
        statusText = findViewById(R.id.statusText)
        debugText = findViewById(R.id.debugText)

        Log.d(TAG, "UI elements initialized")
        updateStatus("TabDisplay - Starting up...", "Initializing components...")

        // Set up debug receiver
        setupDebugReceiver()

        // Hide system bars for full immersion
        hideSystemUI()

        // Initialize surface view
        surfaceView.holder.addCallback(object : SurfaceHolder.Callback {
            override fun surfaceCreated(holder: SurfaceHolder) {
                Log.d(TAG, "Surface created")
                updateStatus("TabDisplay - Surface Ready", "Surface created, notifying service...")

                // Notify service that surface is ready
                val intent = Intent(this@MainActivity, VideoReceiverService::class.java)
                intent.action = VideoReceiverService.ACTION_SURFACE_CREATED
                intent.putExtra(VideoReceiverService.EXTRA_SURFACE, holder.surface)
                startService(intent)
            }

            override fun surfaceChanged(holder: SurfaceHolder, format: Int, width: Int, height: Int) {
                Log.d(TAG, "Surface changed: ${width}x${height}")
                updateStatus("TabDisplay - Surface Ready", "Surface: ${width}x${height}")
            }

            override fun surfaceDestroyed(holder: SurfaceHolder) {
                Log.d(TAG, "Surface destroyed")
                updateStatus("TabDisplay - Surface Lost", "Surface destroyed")

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
            Log.d(TAG, "First run, showing permission dialog")
            updateStatus("TabDisplay - First Run", "Requesting permissions...")
            showPermissionDialog()
        } else {
            Log.d(TAG, "Not first run, starting service")
            updateStatus("TabDisplay - Starting Service", "Loading video service...")
            startVideoReceiverService()
        }

        Log.d(TAG, "MainActivity onCreate completed")
    }

    private fun updateStatus(status: String, debug: String) {
        runOnUiThread {
            statusText.text = status
            addDebugLine(debug)
            addDebugLine("Check logs with: adb logcat | grep TabDisplay")
            Log.d(TAG, "Status: $status | Debug: $debug")
        }
    }
    
    private fun setupDebugReceiver() {
        // Simplified - just show basic status for now
        addDebugLine("Debug system initialized")
        addDebugLine("Waiting for video service to start...")
    }
    
    private fun addDebugLine(line: String) {
        debugHandler.post {
            debugLines.add(line)
            if (debugLines.size > maxDebugLines) {
                debugLines.removeAt(0)
            }
            debugText.text = debugLines.joinToString("\n")
        }
    }
    
    private fun sendDebugBroadcast(message: String, level: String = "INFO") {
        val intent = Intent(ACTION_DEBUG_UPDATE).apply {
            putExtra(EXTRA_DEBUG_MESSAGE, message)
            putExtra(EXTRA_DEBUG_LEVEL, level)
        }
        sendBroadcast(intent)
    }

    override fun onDestroy() {
        super.onDestroy()
        Log.d(TAG, "MainActivity onDestroy")
        
        // Nothing to unregister in simplified version
        
        // Don't stop service here, since we want it to keep running in the background
    }

    private fun hideSystemUI() {
        Log.d(TAG, "Hiding system UI")
        WindowCompat.setDecorFitsSystemWindows(window, false)
        WindowInsetsControllerCompat(window, window.decorView).let { controller ->
            controller.hide(WindowInsetsCompat.Type.systemBars())
            controller.systemBarsBehavior =
                WindowInsetsControllerCompat.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE
        }
    }

    private fun showPermissionDialog() {
        Log.d(TAG, "Showing permission dialog")
        AlertDialog.Builder(this)
            .setTitle(R.string.permission_dialog_title)
            .setMessage(R.string.permission_dialog_message)
            .setPositiveButton(R.string.permission_dialog_ok) { _, _ ->
                Log.d(TAG, "User accepted permissions")
                updateStatus("TabDisplay - Permissions Granted", "Starting service...")

                // Mark first run as completed
                getPreferences(MODE_PRIVATE).edit()
                    .putBoolean(KEY_FIRST_RUN_COMPLETED, true)
                    .apply()

                // Start the service
                startVideoReceiverService()
            }
            .setNegativeButton(R.string.permission_dialog_cancel) { _, _ ->
                Log.d(TAG, "User denied permissions")
                updateStatus("TabDisplay - Permission Denied", "App closing...")
                finish()
            }
            .setCancelable(false)
            .show()
    }

    private fun startVideoReceiverService() {
        Log.d(TAG, "Starting VideoReceiverService and DiscoveryService")
        updateStatus("TabDisplay - Services Starting", "Initializing video receiver and discovery...")

        // Start VideoReceiverService
        val videoIntent = Intent(this, VideoReceiverService::class.java)
        videoIntent.action = VideoReceiverService.ACTION_START

        // Start DiscoveryService
        val discoveryIntent = Intent(this, DiscoveryService::class.java)
        discoveryIntent.action = DiscoveryService.ACTION_START

        try {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                startForegroundService(videoIntent)
                startForegroundService(discoveryIntent)
                Log.d(TAG, "Started foreground services (video + discovery)")
            } else {
                startService(videoIntent)
                startService(discoveryIntent)
                Log.d(TAG, "Started regular services (video + discovery)")
            }
            updateStatus("TabDisplay - Services Started", "Waiting for host discovery...")
        } catch (e: Exception) {
            Log.e(TAG, "Failed to start services", e)
            updateStatus("TabDisplay - Service Error", "Failed to start: ${e.message}")
        }
    }

    private fun setupTouchListener() {
        Log.d(TAG, "Setting up touch listener")
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

            Log.v(TAG, "Touch event: $action at ($normalizedX, $normalizedY)")

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
}
