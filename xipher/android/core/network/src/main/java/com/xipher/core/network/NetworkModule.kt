package com.xipher.core.network

import com.jakewharton.retrofit2.converter.kotlinx.serialization.asConverterFactory
import com.xipher.core.common.AppDispatchers
import com.xipher.core.common.FeatureFlags
import dagger.Module
import dagger.Provides
import dagger.hilt.InstallIn
import dagger.hilt.components.SingletonComponent
import okhttp3.MediaType.Companion.toMediaType
import okhttp3.OkHttpClient
import okhttp3.logging.HttpLoggingInterceptor
import retrofit2.Retrofit
import java.util.concurrent.TimeUnit
import javax.inject.Singleton

@Module
@InstallIn(SingletonComponent::class)
object NetworkModule {
    @Provides
    @Singleton
    fun provideDispatchers(): AppDispatchers = AppDispatchers()

    @Provides
    @Singleton
    fun provideNetworkConfig(): NetworkConfig {
        return NetworkConfig(BuildConfig.DEFAULT_BASE_URL)
    }

    @Provides
    @Singleton
    fun provideOkHttpClient(config: NetworkConfig): OkHttpClient {
        val logging = HttpLoggingInterceptor().apply {
            level = HttpLoggingInterceptor.Level.BASIC
        }
        return OkHttpClient.Builder()
            .addInterceptor(BaseUrlInterceptor(config))
            .addInterceptor(logging)
            .connectTimeout(15, TimeUnit.SECONDS)
            .readTimeout(30, TimeUnit.SECONDS)
            .writeTimeout(30, TimeUnit.SECONDS)
            .build()
    }

    @Provides
    @Singleton
    fun provideRetrofit(client: OkHttpClient, config: NetworkConfig): Retrofit {
        val contentType = "application/json".toMediaType()
        return Retrofit.Builder()
            .baseUrl(config.baseUrl)
            .client(client)
            .addConverterFactory(XipherJson.asConverterFactory(contentType))
            .build()
    }

    @Provides
    @Singleton
    fun provideApiService(retrofit: Retrofit): ApiService = retrofit.create(ApiService::class.java)

    @Provides
    @Singleton
    fun provideApiClient(service: ApiService, dispatchers: AppDispatchers): ApiClient {
        return ApiClient(service, dispatchers.io)
    }

    @Provides
    @Singleton
    fun provideFeatureFlags(): FeatureFlags = FeatureFlags()
}
