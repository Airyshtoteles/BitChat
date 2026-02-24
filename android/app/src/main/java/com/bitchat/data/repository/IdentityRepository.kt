package com.bitchat.data.repository

import android.content.Context
import android.content.SharedPreferences
import android.util.Base64
import androidx.security.crypto.EncryptedSharedPreferences
import androidx.security.crypto.MasterKeys
import com.bitchat.core.BitChatCore
import com.bitchat.domain.model.UserIdentity
import com.bitchat.domain.util.PubKeyUtils
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.withContext

class IdentityRepository(context: Context) {

    private val prefs: SharedPreferences by lazy {
        EncryptedSharedPreferences.create(
            "bitchat_identity",
            MasterKeys.getOrCreate(MasterKeys.AES256_GCM_SPEC),
            context,
            EncryptedSharedPreferences.PrefKeyEncryptionScheme.AES256_SIV,
            EncryptedSharedPreferences.PrefValueEncryptionScheme.AES256_GCM
        )
    }

    private val _identity = MutableStateFlow<UserIdentity?>(null)
    val identity: StateFlow<UserIdentity?> = _identity.asStateFlow()

    fun hasIdentity(): Boolean = prefs.contains(KEY_IDENTITY_DATA)

    suspend fun loadOrCreateIdentity(): Result<UserIdentity> = withContext(Dispatchers.IO) {
        val stored = prefs.getString(KEY_IDENTITY_DATA, null)
        if (stored != null) {
            val data = Base64.decode(stored, Base64.NO_WRAP)
            val rc = BitChatCore.loadIdentity(data)
            if (rc != 0) {
                return@withContext Result.failure(RuntimeException("loadIdentity failed: $rc"))
            }
            val identity = bytesToIdentity(data)
            _identity.value = identity
            return@withContext Result.success(identity)
        }

        val data = BitChatCore.generateIdentity()
            ?: return@withContext Result.failure(RuntimeException("generateIdentity returned null"))

        prefs.edit()
            .putString(KEY_IDENTITY_DATA, Base64.encodeToString(data, Base64.NO_WRAP))
            .apply()

        val identity = bytesToIdentity(data)
        _identity.value = identity
        Result.success(identity)
    }

    private fun bytesToIdentity(data: ByteArray): UserIdentity {
        val pubKey = data.copyOfRange(0, 32)
        val secretKey = data.copyOfRange(32, 64)
        return UserIdentity(
            pubKey = pubKey,
            secretKey = secretKey,
            displayName = PubKeyUtils.generateDisplayName(pubKey),
            avatarColor = PubKeyUtils.generateAvatarColor(pubKey)
        )
    }

    companion object {
        private const val KEY_IDENTITY_DATA = "identity_data"
    }
}
