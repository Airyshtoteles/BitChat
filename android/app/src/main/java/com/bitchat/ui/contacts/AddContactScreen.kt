package com.bitchat.ui.contacts

import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.filled.ArrowBack
import androidx.compose.material.icons.filled.QrCode
import androidx.compose.material.icons.filled.QrCodeScanner
import androidx.compose.material3.Button
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Scaffold
import androidx.compose.material3.SnackbarHost
import androidx.compose.material3.SnackbarHostState
import androidx.compose.material3.Text
import androidx.compose.material3.TopAppBar
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import androidx.hilt.navigation.compose.hiltViewModel
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import androidx.navigation.NavController
import com.bitchat.R

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun AddContactScreen(
    onScanQr: () -> Unit,
    onShowMyQr: () -> Unit,
    onContactAdded: (String) -> Unit,
    onBack: () -> Unit,
    navController: NavController? = null,
    viewModel: ContactsViewModel = hiltViewModel()
) {
    var pubKeyHex by rememberSaveable { mutableStateOf("") }
    var alias by rememberSaveable { mutableStateOf("") }
    val snackbarHostState = remember { SnackbarHostState() }
    val errorMsg = stringResource(R.string.error_invalid_key)

    // Handle QR scan result
    navController?.currentBackStackEntry?.savedStateHandle?.let { handle ->
        val scanned by handle.getStateFlow("scanned_pubkey_hex", "")
            .collectAsStateWithLifecycle()
        LaunchedEffect(scanned) {
            if (scanned.isNotEmpty()) {
                pubKeyHex = scanned
                handle["scanned_pubkey_hex"] = ""
            }
        }
    }

    Scaffold(
        topBar = {
            TopAppBar(
                navigationIcon = {
                    IconButton(onClick = onBack) {
                        Icon(
                            imageVector = Icons.AutoMirrored.Filled.ArrowBack,
                            contentDescription = stringResource(R.string.back)
                        )
                    }
                },
                title = { Text(text = stringResource(R.string.add_contact_title)) }
            )
        },
        snackbarHost = { SnackbarHost(snackbarHostState) }
    ) { padding ->
        Column(
            modifier = Modifier
                .fillMaxSize()
                .padding(padding)
                .padding(16.dp)
        ) {
            Row(modifier = Modifier.fillMaxWidth()) {
                OutlinedButton(
                    onClick = onScanQr,
                    modifier = Modifier.weight(1f)
                ) {
                    Icon(
                        imageVector = Icons.Default.QrCodeScanner,
                        contentDescription = null
                    )
                    Spacer(modifier = Modifier.width(8.dp))
                    Text(text = stringResource(R.string.add_contact_scan_qr))
                }

                Spacer(modifier = Modifier.width(8.dp))

                OutlinedButton(
                    onClick = onShowMyQr,
                    modifier = Modifier.weight(1f)
                ) {
                    Icon(
                        imageVector = Icons.Default.QrCode,
                        contentDescription = null
                    )
                    Spacer(modifier = Modifier.width(8.dp))
                    Text(text = stringResource(R.string.add_contact_show_qr))
                }
            }

            Spacer(modifier = Modifier.height(24.dp))

            HorizontalDivider()

            Spacer(modifier = Modifier.height(24.dp))

            Text(
                text = stringResource(R.string.add_contact_manual),
                style = MaterialTheme.typography.titleMedium
            )

            Spacer(modifier = Modifier.height(12.dp))

            OutlinedTextField(
                value = pubKeyHex,
                onValueChange = { pubKeyHex = it.filter { c -> c in "0123456789abcdefABCDEF" }.take(64) },
                label = { Text(text = stringResource(R.string.add_contact_hex_hint)) },
                modifier = Modifier.fillMaxWidth(),
                singleLine = true,
                supportingText = { Text("${pubKeyHex.length}/64") }
            )

            Spacer(modifier = Modifier.height(8.dp))

            OutlinedTextField(
                value = alias,
                onValueChange = { alias = it },
                label = { Text(text = stringResource(R.string.add_contact_alias_hint)) },
                modifier = Modifier.fillMaxWidth(),
                singleLine = true
            )

            Spacer(modifier = Modifier.height(16.dp))

            Button(
                onClick = {
                    val success = viewModel.addContact(pubKeyHex.lowercase(), alias)
                    if (success) {
                        onContactAdded(pubKeyHex.lowercase())
                    }
                },
                modifier = Modifier.fillMaxWidth(),
                enabled = pubKeyHex.length == 64
            ) {
                Text(text = stringResource(R.string.add_contact_button))
            }
        }
    }
}
