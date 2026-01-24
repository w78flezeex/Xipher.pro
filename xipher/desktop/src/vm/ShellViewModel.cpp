#include "vm/ShellViewModel.h"

#include <QClipboard>
#include <QGuiApplication>

ShellViewModel::ShellViewModel(ChatStore* store, QObject* parent) : QObject(parent), store_(store) {
    if (store_) {
        connect(store_, &ChatStore::selectedChatIdChanged, this, &ShellViewModel::selectedChatIdChanged);
        connect(store_, &ChatStore::selectedChatTitleChanged, this,
                &ShellViewModel::selectedChatTitleChanged);
        connect(store_, &ChatStore::selectedChatOnlineChanged, this,
                &ShellViewModel::selectedChatOnlineChanged);
        connect(store_, &ChatStore::typingIndicatorChanged, this,
                &ShellViewModel::typingIndicatorChanged);
        connect(store_, &ChatStore::loadingChatsChanged, this, &ShellViewModel::loadingChatsChanged);
        connect(store_, &ChatStore::loadingMessagesChanged, this,
                &ShellViewModel::loadingMessagesChanged);
        connect(store_, &ChatStore::offlineChanged, this, &ShellViewModel::offlineChanged);
        connect(store_, &ChatStore::errorMessageChanged, this, &ShellViewModel::errorMessageChanged);
        connect(store_, &ChatStore::toastRequested, this, &ShellViewModel::toastRequested);
        connect(store_, &ChatStore::userFound, this, &ShellViewModel::userFound);
        connect(store_, &ChatStore::groupCreated, this, &ShellViewModel::groupCreated);
    }
}

ChatListModel* ShellViewModel::chatListModel() {
    return store_ ? store_->chatListModel() : nullptr;
}

MessageListModel* ShellViewModel::messageListModel() {
    return store_ ? store_->messageListModel() : nullptr;
}

UploadListModel* ShellViewModel::uploadListModel() {
    return store_ ? store_->uploadListModel() : nullptr;
}

QString ShellViewModel::selectedChatId() const {
    return store_ ? store_->selectedChatId() : QString();
}

QString ShellViewModel::selectedChatTitle() const {
    return store_ ? store_->selectedChatTitle() : QString();
}

bool ShellViewModel::selectedChatOnline() const {
    return store_ ? store_->selectedChatOnline() : false;
}

QString ShellViewModel::typingIndicator() const {
    return store_ ? store_->typingIndicator() : QString();
}

bool ShellViewModel::loadingChats() const {
    return store_ ? store_->loadingChats() : false;
}

bool ShellViewModel::loadingMessages() const {
    return store_ ? store_->loadingMessages() : false;
}

bool ShellViewModel::offline() const {
    return store_ ? store_->offline() : false;
}

QString ShellViewModel::errorMessage() const {
    return store_ ? store_->errorMessage() : QString();
}

QString ShellViewModel::searchQuery() const {
    return searchQuery_;
}

void ShellViewModel::setSearchQuery(const QString& query) {
    if (searchQuery_ == query) {
        return;
    }
    searchQuery_ = query;
    emit searchQueryChanged();
    if (store_) {
        store_->setSearchQuery(searchQuery_);
    }
}

void ShellViewModel::selectChat(const QString& chatId) {
    if (store_) {
        store_->selectChat(chatId);
    }
}

void ShellViewModel::sendMessage(const QString& text) {
    if (store_) {
        store_->sendMessage(text);
    }
}

void ShellViewModel::addAttachment(const QString& filePath) {
    if (store_) {
        store_->addAttachment(filePath);
    }
}

void ShellViewModel::cancelUpload(const QString& tempId) {
    if (store_) {
        store_->cancelUpload(tempId);
    }
}

void ShellViewModel::retryMessage(const QString& messageId) {
    if (store_) {
        store_->retryMessage(messageId);
    }
}

void ShellViewModel::deleteMessage(const QString& messageId) {
    if (store_) {
        store_->deleteMessage(messageId);
    }
}

void ShellViewModel::copyToClipboard(const QString& text) {
    if (text.isEmpty()) {
        return;
    }
    QClipboard* clipboard = QGuiApplication::clipboard();
    if (clipboard) {
        clipboard->setText(text);
    }
}

void ShellViewModel::refreshChats() {
    if (store_) {
        store_->refreshChats();
    }
}

void ShellViewModel::sendTyping(bool isTyping) {
    if (store_) {
        store_->sendTyping(isTyping);
    }
}

void ShellViewModel::findUser(const QString& username) {
    if (store_) {
        store_->findUser(username);
    }
}

void ShellViewModel::createGroup(const QString& name, const QStringList& memberIds) {
    if (store_) {
        store_->createGroup(name, memberIds);
    }
}
