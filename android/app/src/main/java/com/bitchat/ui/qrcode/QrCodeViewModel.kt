package com.bitchat.ui.qrcode

import android.graphics.Bitmap
import androidx.lifecycle.ViewModel
import com.bitchat.data.repository.ContactRepository
import com.bitchat.data.repository.IdentityRepository
import com.bitchat.domain.util.PubKeyUtils
import com.google.zxing.BarcodeFormat
import com.google.zxing.EncodeHintType
import com.google.zxing.MultiFormatWriter
import dagger.hilt.android.lifecycle.HiltViewModel
import kotlinx.coroutines.flow.SharingStarted
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.map
import kotlinx.coroutines.flow.stateIn
import javax.inject.Inject
import androidx.lifecycle.viewModelScope
import kotlinx.coroutines.launch

@HiltViewModel
class QrCodeViewModel @Inject constructor(
    private val identityRepository: IdentityRepository,
    private val contactRepository: ContactRepository
) : ViewModel() {

    val myPubKeyHex: StateFlow<String?> = identityRepository.identity
        .map { it?.pubKey?.let { key -> PubKeyUtils.toHex(key) } }
        .stateIn(viewModelScope, SharingStarted.WhileSubscribed(5000), null)

    fun generateQrBitmap(content: String, size: Int = 512): Bitmap {
        val hints = mapOf(EncodeHintType.MARGIN to 1)
        val bitMatrix = MultiFormatWriter().encode(
            "bitchat:$content",
            BarcodeFormat.QR_CODE,
            size,
            size,
            hints
        )
        val bitmap = Bitmap.createBitmap(size, size, Bitmap.Config.ARGB_8888)
        for (x in 0 until size) {
            for (y in 0 until size) {
                bitmap.setPixel(
                    x, y,
                    if (bitMatrix[x, y]) android.graphics.Color.BLACK
                    else android.graphics.Color.WHITE
                )
            }
        }
        return bitmap
    }

    fun parseQrContent(rawValue: String): String? {
        val hex = rawValue.removePrefix("bitchat:")
        if (hex.length != 64) return null
        return try {
            PubKeyUtils.fromHex(hex)
            hex
        } catch (e: Exception) {
            null
        }
    }

    fun addScannedContact(pubKeyHex: String) {
        viewModelScope.launch {
            contactRepository.addContact(PubKeyUtils.fromHex(pubKeyHex))
        }
    }
}
