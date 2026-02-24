package com.bitchat.di

import android.content.Context
import androidx.room.Room
import com.bitchat.data.local.db.BitChatDatabase
import com.bitchat.data.local.db.ChatMessageDao
import com.bitchat.data.local.db.ContactDao
import dagger.Module
import dagger.Provides
import dagger.hilt.InstallIn
import dagger.hilt.android.qualifiers.ApplicationContext
import dagger.hilt.components.SingletonComponent
import javax.inject.Singleton

@Module
@InstallIn(SingletonComponent::class)
object DatabaseModule {

    @Provides
    @Singleton
    fun provideDatabase(@ApplicationContext context: Context): BitChatDatabase =
        Room.databaseBuilder(
            context,
            BitChatDatabase::class.java,
            "bitchat_app.db"
        ).build()

    @Provides
    fun provideContactDao(db: BitChatDatabase): ContactDao = db.contactDao()

    @Provides
    fun provideChatMessageDao(db: BitChatDatabase): ChatMessageDao = db.chatMessageDao()
}
