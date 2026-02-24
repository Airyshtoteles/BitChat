package com.bitchat.data.local.converter

import android.util.Base64
import androidx.room.TypeConverter

class ByteArrayConverter {
    @TypeConverter
    fun fromByteArray(bytes: ByteArray): String =
        Base64.encodeToString(bytes, Base64.NO_WRAP)

    @TypeConverter
    fun toByteArray(encoded: String): ByteArray =
        Base64.decode(encoded, Base64.NO_WRAP)
}
