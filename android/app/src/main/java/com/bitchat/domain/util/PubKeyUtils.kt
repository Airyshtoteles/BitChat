package com.bitchat.domain.util

import androidx.compose.ui.graphics.Color

object PubKeyUtils {

    private val ADJECTIVES = listOf(
        "Brave", "Calm", "Bold", "Keen", "Swift",
        "Noble", "Wise", "Pale", "Dark", "Warm",
        "Bright", "Cool", "Fierce", "Gentle", "Quiet",
        "Vivid", "Steady", "Grand", "Stout", "Agile",
        "Lucky", "Witty", "Proud", "Merry", "Clear",
        "Solid", "Fresh", "Brisk", "Loyal", "Quick",
        "Sharp", "Smart", "Tough", "Deft", "Eager",
        "Fair", "Firm", "Glad", "Hardy", "Kind",
        "Light", "Neat", "Prime", "Rare", "Safe",
        "Sleek", "True", "Vast", "Wild", "Young",
        "Amber", "Azure", "Coral", "Ivory", "Jade",
        "Rusty", "Sandy", "Storm", "Tidal", "Lunar",
        "Solar", "Polar", "Misty", "Frosty"
    )

    private val ANIMALS = listOf(
        "Fox", "Wolf", "Bear", "Hawk", "Lynx",
        "Deer", "Hare", "Crow", "Seal", "Wren",
        "Otter", "Eagle", "Badger", "Falcon", "Raven",
        "Tiger", "Crane", "Panda", "Whale", "Bison",
        "Heron", "Robin", "Finch", "Dingo", "Koala",
        "Moose", "Eland", "Gecko", "Newt", "Shrew",
        "Viper", "Perch", "Trout", "Pike", "Ibis",
        "Kiwi", "Lark", "Moth", "Wasp", "Crab",
        "Dove", "Frog", "Goat", "Mink", "Puma",
        "Swan", "Toad", "Yak", "Mule", "Owl",
        "Colt", "Lamb", "Stag", "Boar", "Hound",
        "Shark", "Squid", "Lemur", "Stoat", "Quail",
        "Snipe", "Egret", "Swift", "Marten"
    )

    fun generateDisplayName(pubKey: ByteArray): String {
        val adjIdx = (pubKey[0].toInt() and 0xFF) % ADJECTIVES.size
        val animalIdx = (pubKey[1].toInt() and 0xFF) % ANIMALS.size
        val suffix = pubKey.take(4).joinToString("") { "%02X".format(it) }.takeLast(4)
        return "${ADJECTIVES[adjIdx]}${ANIMALS[animalIdx]}-$suffix"
    }

    fun generateAvatarColor(pubKey: ByteArray): Long {
        val hue = ((pubKey[2].toInt() and 0xFF) / 255f) * 360f
        val sat = 0.5f + ((pubKey[3].toInt() and 0xFF) / 255f) * 0.3f
        val lum = 0.4f + ((pubKey[4].toInt() and 0xFF) / 255f) * 0.2f
        return Color.hsl(hue, sat, lum).value.toLong()
    }

    fun shortHex(pubKey: ByteArray): String {
        val hex = pubKey.joinToString("") { "%02x".format(it) }
        return "${hex.take(4)}...${hex.takeLast(4)}"
    }

    fun toHex(pubKey: ByteArray): String =
        pubKey.joinToString("") { "%02x".format(it) }

    fun fromHex(hex: String): ByteArray {
        require(hex.length % 2 == 0) { "Hex string must have even length" }
        return ByteArray(hex.length / 2) { i ->
            hex.substring(i * 2, i * 2 + 2).toInt(16).toByte()
        }
    }
}
