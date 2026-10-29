#include "gatewayController.h"
#include <QtConcurrent/QtConcurrentRun>

GatewayController::GatewayController(const QString &, bool, int, bool, SecureAppSettingsRepository *, QObject *parent)
    : QObject(parent)
{
}

GatewayController::~GatewayController() = default;

amnezia::ErrorCode GatewayController::post(const QString &, QJsonObject, QByteArray &body)
{
    body.clear();
    return amnezia::ErrorCode::ApiConfigDownloadError;
}

QFuture<QPair<amnezia::ErrorCode, QByteArray>> GatewayController::postAsync(const QString &, QJsonObject)
{
    return QtConcurrent::run([] { return qMakePair(amnezia::ErrorCode::ApiConfigDownloadError, QByteArray()); });
}
