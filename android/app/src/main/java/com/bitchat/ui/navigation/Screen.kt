package com.bitchat.ui.navigation

sealed class Screen(val route: String) {
    data object Onboarding : Screen("onboarding")
    data object ChatList : Screen("chat_list")
    data object Conversation : Screen("conversation/{pubKeyHex}") {
        fun createRoute(pubKeyHex: String) = "conversation/$pubKeyHex"
    }
    data object AddContact : Screen("add_contact")
    data object ContactList : Screen("contact_list")
    data object QrCode : Screen("qr_code")
    data object QrScanner : Screen("qr_scanner")
    data object Profile : Screen("profile")
}
