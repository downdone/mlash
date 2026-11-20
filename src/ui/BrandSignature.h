#pragma once
#include "TrayController.h"
#include "UiTheme.h"
#include <QHBoxLayout>
#include <QLabel>

namespace mlash {
inline QWidget* makeBrandSignature(QWidget* parent, int iconSize, const QString& wordmarkName) {
    auto* signature = new QWidget(parent);
    auto* layout = new QHBoxLayout(signature);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);

    auto* mark = new QLabel(signature);
    mark->setObjectName("brandMark");
    mark->setPixmap(TrayController::icon().pixmap(iconSize, iconSize));
    mark->setFixedSize(iconSize, iconSize);
    layout->addWidget(mark);

    auto* wordmark = new QLabel(QStringLiteral("Mlash"), signature);
    wordmark->setObjectName(wordmarkName);
    const auto updateFont = [wordmark] {
        auto font = UiTheme::bodyFont();
        font.setPointSizeF(design::ShadowrocketStyle::brandPoints * UiTheme::textScale() / 100.0);
        font.setWeight(QFont::Bold);
        font.setVariableAxis("wght", 700);
        wordmark->setFont(font);
    };
    updateFont();
    QObject::connect(UiTheme::notifier(), &UiTheme::AppearanceNotifier::changed, wordmark, updateFont);
    layout->addWidget(wordmark, 1);
    return signature;
}
}
