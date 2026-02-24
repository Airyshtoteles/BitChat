package com.bitchat.ui.navigation

import androidx.compose.runtime.Composable
import androidx.navigation.NavHostController
import androidx.navigation.NavType
import androidx.navigation.compose.NavHost
import androidx.navigation.compose.composable
import androidx.navigation.navArgument
import com.bitchat.ui.chatlist.ChatListScreen
import com.bitchat.ui.contacts.AddContactScreen
import com.bitchat.ui.contacts.ContactListScreen
import com.bitchat.ui.conversation.ConversationScreen
import com.bitchat.ui.onboarding.OnboardingScreen
import com.bitchat.ui.profile.ProfileScreen
import com.bitchat.ui.qrcode.QrCodeScreen
import com.bitchat.ui.qrcode.QrScannerScreen

@Composable
fun NavGraph(
    navController: NavHostController,
    startDestination: String
) {
    NavHost(navController = navController, startDestination = startDestination) {
        composable(Screen.Onboarding.route) {
            OnboardingScreen(
                onComplete = {
                    navController.navigate(Screen.ChatList.route) {
                        popUpTo(Screen.Onboarding.route) { inclusive = true }
                    }
                }
            )
        }

        composable(Screen.ChatList.route) {
            ChatListScreen(
                onChatClick = { pubKeyHex ->
                    navController.navigate(Screen.Conversation.createRoute(pubKeyHex))
                },
                onAddContact = {
                    navController.navigate(Screen.AddContact.route)
                },
                onProfileClick = {
                    navController.navigate(Screen.Profile.route)
                }
            )
        }

        composable(
            route = Screen.Conversation.route,
            arguments = listOf(navArgument("pubKeyHex") { type = NavType.StringType })
        ) {
            ConversationScreen(
                onBack = { navController.popBackStack() }
            )
        }

        composable(Screen.AddContact.route) {
            AddContactScreen(
                onScanQr = { navController.navigate(Screen.QrScanner.route) },
                onShowMyQr = { navController.navigate(Screen.QrCode.route) },
                onContactAdded = { pubKeyHex ->
                    navController.popBackStack()
                    navController.navigate(Screen.Conversation.createRoute(pubKeyHex))
                },
                onBack = { navController.popBackStack() }
            )
        }

        composable(Screen.ContactList.route) {
            ContactListScreen(
                onContactClick = { pubKeyHex ->
                    navController.navigate(Screen.Conversation.createRoute(pubKeyHex))
                },
                onBack = { navController.popBackStack() }
            )
        }

        composable(Screen.QrCode.route) {
            QrCodeScreen(onBack = { navController.popBackStack() })
        }

        composable(Screen.QrScanner.route) {
            QrScannerScreen(
                onScanned = { pubKeyHex ->
                    navController.previousBackStackEntry?.savedStateHandle?.set(
                        "scanned_pubkey_hex", pubKeyHex
                    )
                    navController.popBackStack()
                },
                onBack = { navController.popBackStack() }
            )
        }

        composable(Screen.Profile.route) {
            ProfileScreen(
                onShowQr = { navController.navigate(Screen.QrCode.route) },
                onBack = { navController.popBackStack() }
            )
        }
    }
}
