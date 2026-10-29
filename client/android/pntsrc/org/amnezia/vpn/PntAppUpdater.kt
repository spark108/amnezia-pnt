package org.amnezia.vpn

import android.content.Context
import android.content.Intent
import android.net.Uri
import android.provider.Settings
import androidx.core.content.FileProvider
import java.io.File

@androidx.annotation.Keep
object PntAppUpdater {
    @JvmStatic fun install(context: Context, path: String): String {
        return try {
            val file = File(path).canonicalFile
            if (!file.isFile || !file.toPath().startsWith(context.filesDir.canonicalFile.toPath())) {
                return "Invalid update path"
            }
            val info = context.packageManager.getPackageArchiveInfo(file.path, 0)
            val installed = context.packageManager.getPackageInfo(context.packageName, 0)
            if (info?.packageName != context.packageName || info.longVersionCode <= installed.longVersionCode) {
                return "Invalid package or version"
            }
            if (!context.packageManager.canRequestPackageInstalls()) {
                context.startActivity(Intent(Settings.ACTION_MANAGE_UNKNOWN_APP_SOURCES, Uri.parse("package:${context.packageName}"))
                    .addFlags(Intent.FLAG_ACTIVITY_NEW_TASK))
                return "permission"
            }
            val uri = FileProvider.getUriForFile(context, "${context.packageName}.qtprovider", file)
            context.startActivity(Intent(Intent.ACTION_VIEW)
                .setDataAndType(uri, "application/vnd.android.package-archive")
                .addFlags(Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_GRANT_READ_URI_PERMISSION))
            ""
        } catch (error: Exception) {
            error.javaClass.simpleName
        }
    }
}
