package com.bitchat.service

import android.app.Notification
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.PendingIntent
import android.app.Service
import android.content.Context
import android.content.Intent
import android.os.IBinder
import android.util.Log
import androidx.core.app.NotificationCompat
import androidx.core.content.ContextCompat
import com.bitchat.R
import com.bitchat.core.BitChatCore
import com.bitchat.data.repository.IdentityRepository
import com.bitchat.data.repository.MessageRepository
import dagger.hilt.android.AndroidEntryPoint
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.cancel
import kotlinx.coroutines.delay
import kotlinx.coroutines.isActive
import kotlinx.coroutines.launch
import javax.inject.Inject

@AndroidEntryPoint
class BitChatService : Service() {

    @Inject
    lateinit var messageRepository: MessageRepository

    @Inject
    lateinit var identityRepository: IdentityRepository

    private val serviceScope = CoroutineScope(SupervisorJob() + Dispatchers.IO)
    private var tickJob: Job? = null

    override fun onCreate() {
        super.onCreate()
        createNotificationChannel()
        startForeground(NOTIFICATION_ID, buildNotification())
    }

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        initializeCoreAndStartTicking()
        return START_STICKY
    }

    private fun initializeCoreAndStartTicking() {
        serviceScope.launch {
            val dbPath = filesDir.absolutePath + "/bitchat.db"
            val rc = BitChatCore.init(dbPath)
            if (rc != 0) {
                Log.e(TAG, "BitChatCore.init failed: $rc")
                return@launch
            }

            identityRepository.loadOrCreateIdentity()

            BitChatCore.setMessageListener(object : BitChatCore.MessageListener {
                override fun onMessageReceived(senderPubKey: ByteArray, payload: ByteArray) {
                    serviceScope.launch {
                        messageRepository.receiveMessage(senderPubKey, payload)
                    }
                }
            })

            BitChatCore.startDiscovery()

            tickJob = launch {
                while (isActive) {
                    BitChatCore.tick()
                    delay(TICK_INTERVAL_MS)
                }
            }
        }
    }

    override fun onDestroy() {
        tickJob?.cancel()
        serviceScope.cancel()
        BitChatCore.setMessageListener(null)
        BitChatCore.shutdown()
        super.onDestroy()
    }

    override fun onBind(intent: Intent?): IBinder? = null

    private fun createNotificationChannel() {
        val channel = NotificationChannel(
            CHANNEL_ID,
            getString(R.string.notification_channel_name),
            NotificationManager.IMPORTANCE_LOW
        ).apply {
            description = getString(R.string.notification_content)
        }
        getSystemService(NotificationManager::class.java)
            .createNotificationChannel(channel)
    }

    private fun buildNotification(): Notification {
        val pendingIntent = PendingIntent.getActivity(
            this, 0,
            packageManager.getLaunchIntentForPackage(packageName),
            PendingIntent.FLAG_IMMUTABLE
        )

        return NotificationCompat.Builder(this, CHANNEL_ID)
            .setContentTitle(getString(R.string.app_name))
            .setContentText(getString(R.string.notification_content))
            .setSmallIcon(R.drawable.ic_launcher_foreground)
            .setContentIntent(pendingIntent)
            .setOngoing(true)
            .build()
    }

    companion object {
        private const val TAG = "BitChatService"
        private const val NOTIFICATION_ID = 1
        private const val CHANNEL_ID = "bitchat_service"
        private const val TICK_INTERVAL_MS = 5000L

        fun start(context: Context) {
            val intent = Intent(context, BitChatService::class.java)
            ContextCompat.startForegroundService(context, intent)
        }

        fun stop(context: Context) {
            context.stopService(Intent(context, BitChatService::class.java))
        }
    }
}
