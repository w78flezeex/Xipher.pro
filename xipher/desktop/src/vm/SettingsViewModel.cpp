#include "vm/SettingsViewModel.h"

SettingsViewModel::SettingsViewModel(SettingsStore* settings, AppConfig* config, ApiClient* api,
                                     Session* session, QObject* parent)
    : QObject(parent), settings_(settings), config_(config), api_(api), session_(session) {
    if (settings_) {
        connect(settings_, &SettingsStore::baseUrlChanged, this, &SettingsViewModel::baseUrlChanged);
        connect(settings_, &SettingsStore::themeChanged, this, &SettingsViewModel::themeChanged);
        connect(settings_, &SettingsStore::languageChanged, this, &SettingsViewModel::languageChanged);
    }
    if (api_) {
        connect(api_, &ApiClient::sessionValidated, this, [this](const ApiDtos::SessionResponse& resp) {
            if (!resp.success) {
                setStatusMessage(resp.message.isEmpty() ? "Session invalid" : resp.message);
                return;
            }
            if (session_) {
                session_->setSession(resp.userId, resp.username, session_->token());
            }
            setStatusMessage("Session validated");
        });
        
        connect(api_, &ApiClient::userProfileFetched, this, [this](const ApiDtos::UserProfileResponse& resp) {
            profileLoading_ = false;
            emit profileLoadingChanged();
            
            if (!resp.success) {
                setStatusMessage(resp.message.isEmpty() ? "Failed to load profile" : resp.message);
                return;
            }
            
            firstName_ = resp.firstName;
            lastName_ = resp.lastName;
            bio_ = resp.bio;
            avatarUrl_ = resp.avatarUrl;
            isPremium_ = resp.isPremium;
            premiumPlan_ = resp.premiumPlan;
            
            emit firstNameChanged();
            emit lastNameChanged();
            emit bioChanged();
            emit avatarUrlChanged();
            emit isPremiumChanged();
            emit premiumPlanChanged();
        });
        
        connect(api_, &ApiClient::userProfileUpdated, this, [this](bool success, const QString& message) {
            emit profileSaved(success, message);
            setStatusMessage(message);
        });
    }
}

QString SettingsViewModel::baseUrl() const {
    return settings_ ? settings_->baseUrl() : QString();
}

void SettingsViewModel::setBaseUrl(const QString& baseUrl) {
    if (settings_) {
        settings_->setBaseUrl(baseUrl);
    }
    if (config_) {
        config_->setBaseUrl(baseUrl);
    }
    setStatusMessage("Base URL updated");
}

QString SettingsViewModel::theme() const {
    return settings_ ? settings_->theme() : QString("dark");
}

void SettingsViewModel::setTheme(const QString& theme) {
    if (settings_) {
        settings_->setTheme(theme);
    }
}

QString SettingsViewModel::language() const {
    return settings_ ? settings_->language() : QString("en");
}

void SettingsViewModel::setLanguage(const QString& language) {
    if (settings_) {
        settings_->setLanguage(language);
    }
}

QString SettingsViewModel::statusMessage() const {
    return statusMessage_;
}

// Profile getters/setters
QString SettingsViewModel::firstName() const { return firstName_; }
void SettingsViewModel::setFirstName(const QString& value) {
    if (firstName_ != value) { firstName_ = value; emit firstNameChanged(); }
}

QString SettingsViewModel::lastName() const { return lastName_; }
void SettingsViewModel::setLastName(const QString& value) {
    if (lastName_ != value) { lastName_ = value; emit lastNameChanged(); }
}

QString SettingsViewModel::bio() const { return bio_; }
void SettingsViewModel::setBio(const QString& value) {
    if (bio_ != value) { bio_ = value; emit bioChanged(); }
}

QString SettingsViewModel::avatarUrl() const { return avatarUrl_; }
bool SettingsViewModel::isPremium() const { return isPremium_; }
QString SettingsViewModel::premiumPlan() const { return premiumPlan_; }
bool SettingsViewModel::profileLoading() const { return profileLoading_; }

// Notification settings
bool SettingsViewModel::desktopNotifications() const { 
    return settings_ ? settings_->desktopNotifications() : true; 
}
void SettingsViewModel::setDesktopNotifications(bool value) {
    if (settings_) settings_->setDesktopNotifications(value);
    emit desktopNotificationsChanged();
}

bool SettingsViewModel::soundNotifications() const { 
    return settings_ ? settings_->soundNotifications() : true; 
}
void SettingsViewModel::setSoundNotifications(bool value) {
    if (settings_) settings_->setSoundNotifications(value);
    emit soundNotificationsChanged();
}

bool SettingsViewModel::showPreview() const { 
    return settings_ ? settings_->showPreview() : true; 
}
void SettingsViewModel::setShowPreview(bool value) {
    if (settings_) settings_->setShowPreview(value);
    emit showPreviewChanged();
}

void SettingsViewModel::revalidateSession() {
    if (api_) {
        api_->validateToken();
    }
}

void SettingsViewModel::loadProfile() {
    if (!api_ || !session_ || session_->userId().isEmpty()) {
        return;
    }
    profileLoading_ = true;
    emit profileLoadingChanged();
    
    api_->getUserProfile(session_->userId());
}

void SettingsViewModel::saveProfile() {
    if (!api_ || !session_) {
        return;
    }
    api_->updateUserProfile(firstName_, lastName_, bio_);
}

void SettingsViewModel::setStatusMessage(const QString& message) {
    if (statusMessage_ == message) {
        return;
    }
    statusMessage_ = message;
    emit statusMessageChanged();
}
