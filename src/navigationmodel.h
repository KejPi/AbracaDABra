/*
 * This file is part of the AbracaDABra project
 *
 * MIT License
 *
 * Copyright (c) 2019-2026 Petr Kopecký <xkejpi (at) gmail (dot) com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef NAVIGATIONMODEL_H
#define NAVIGATIONMODEL_H

#include <QAbstractListModel>
#include <QObject>
#include <QtQmlIntegration>

class NavigationModelItem;

class NavigationModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("NavigationModel cannot be instantiated")
    Q_PROPERTY(NavigationModel::NavigationId currentId READ currentId WRITE setCurrentId NOTIFY currentIdChanged FINAL)
public:
    enum NavigationId
    {  // DO NOT CHANGE order of items, it would break settings
        Undefined = -1,
        Service = 0,
        EnsembleInfo,
        Tii,
        Scanner,
        Epg,
        DabSignal,
        ServiceList,
        CatSls,
        Settings,
        BandScan,
        ExportServiceList,
        ClearServiceList,
        AppLog,
        About,
        AudioRecordingSchedule,
        AudioRecording,
        ShowMore,
        Quit,
        // DO NOT CHANGE order of items, it would break settings
    };
    Q_ENUM(NavigationId);

    enum NavigationModelRoles
    {
        FunctionalityIdRole = Qt::UserRole,
        ShortLabelRole,
        LongLabelRole,
        IconNameRole,
        QmlComponentPathRole,
        IsEnabledRole,
        IsUndockedRole,
        CanUndockRole,
        IsSeparatorRole,
        UndockedWidth,
        UndockedHeight,
        UndockedX,
        UndockedY,
        OptionsRole,
        ItemRole
    };

    enum NavigationOption
    {
        NoneOption = 0x0,
        Landscape1Option = 0x0001,
        Landscape2Option = 0x0002,
        LandscapeOthersOption = 0x004,
        PortraitOption = 0x0008,
        PortraitOthersOption = 0x0010,
        LandscapeSmallOption = 0x0020,
        LandscapeSmallOthersOption = 0x0040,
        PortraitSmallOption = 0x0080,
        PortraitSmallOthersOption = 0x0100,
        OthersOption = LandscapeOthersOption | PortraitOthersOption | LandscapeSmallOthersOption | PortraitSmallOthersOption,
        PortraitAnyOption = PortraitOption | PortraitSmallOption | PortraitOthersOption | PortraitSmallOthersOption,
        LandscapeAnyOption = Landscape1Option | Landscape2Option | LandscapeSmallOption | LandscapeOthersOption | LandscapeSmallOthersOption,

        EnabledOption = 0x0200,
        UndockableOption = 0x0400,
        UndockedOption = 0x0800,
        SeparatorOption = 0x1000,

        AllOption = 0xFFFF
    };
    Q_ENUM(NavigationOption);
    Q_DECLARE_FLAGS(NavigationOptions, NavigationOption)

    explicit NavigationModel(QObject *parent = nullptr);
    ~NavigationModel();
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override { return m_modelData.count(); }
    QHash<int, QByteArray> roleNames() const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override { return Qt::ItemIsEditable; }

    NavigationModel::NavigationId currentId() const;
    void setCurrentId(NavigationModel::NavigationId newCurrentId);

    void setEnabled(NavigationModel::NavigationId id, bool enabled);
    void setLabel(NavigationModel::NavigationId id, const QString &longLabel, const QString &shortLabel = QString());

    bool isActive(NavigationModel::NavigationId id) const;

    Q_INVOKABLE void alignIdForCurrentView(bool isPortrait);
    Q_INVOKABLE QVariantMap getFunctionalityItemData(NavigationModel::NavigationId id) const;
    const NavigationModelItem *getItem(int row) const;

    void loadSettings(QSettings *settings);
    void saveSettings(QSettings *settings);

signals:
    void currentIdChanged();
    void isUndockedChanged(NavigationModel::NavigationId id, bool isUndocked);
    void isActiveChanged(NavigationModel::NavigationId id, bool isActive);

private:
    QList<NavigationModelItem *> m_modelData;
    NavigationId m_currentId = NavigationId::Undefined;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(NavigationModel::NavigationOptions)

class NavigationModelItem
{
public:
    explicit NavigationModelItem(NavigationModel::NavigationId id, const QString &shortLabel, const QString &longLabel,
                                 const QString &qmlComponentPath, const QString &iconName, NavigationModel::NavigationOptions options)
        : m_id(id), m_shortLabel(shortLabel), m_longLabel(longLabel), m_qmlComponentPath(qmlComponentPath), m_iconName(iconName), m_options(options)
    {}

    NavigationModel::NavigationId id() const { return m_id; }

    QString shortLabel() const { return m_shortLabel; }
    void setShortLabel(const QString &newShortLabel) { m_shortLabel = newShortLabel; }

    QString longLabel() const { return m_longLabel; }
    void setLongLabel(const QString &newLongLabel) { m_longLabel = newLongLabel; }

    QString iconName() const { return m_iconName; }
    void setIconName(const QString &newIconName) { m_iconName = newIconName; }

    bool isEnabled() const { return ((m_options & NavigationModel::NavigationOption::EnabledOption) != 0); }
    void setIsEnabled(bool newIsEnabled)
    {
        if (newIsEnabled)
        {
            m_options |= NavigationModel::NavigationOption::EnabledOption;
        }
        else
        {
            m_options &= ~NavigationModel::NavigationOption::EnabledOption;
        }
    }
    bool isUndockable() const { return ((m_options & NavigationModel::NavigationOption::UndockableOption) != 0); }

    bool isUndocked() const { return ((m_options & NavigationModel::NavigationOption::UndockedOption) != 0); }
    void setIsUndocked(bool newIsUndocked)
    {
        if (newIsUndocked)
        {
            m_options |= NavigationModel::NavigationOption::UndockedOption;
        }
        else
        {
            m_options &= ~NavigationModel::NavigationOption::UndockedOption;
        }
    }
    bool isSeparator() const { return ((m_options & NavigationModel::NavigationOption::SeparatorOption) != 0); }

    QFlags<NavigationModel::NavigationOption> options() const { return m_options; }
    void setOptions(const QFlags<NavigationModel::NavigationOption> &options) { m_options = options; }

    QString qmlComponentPath() const { return m_qmlComponentPath; }
    void setQmlComponentPath(const QString &newQmlComponentPath) { m_qmlComponentPath = newQmlComponentPath; }

    int undockedWidth() const { return m_undockedWidth; }
    void setUndockedWidth(int newUndockedWidth) { m_undockedWidth = newUndockedWidth; }

    int undockedHeight() const { return m_undockedHeight; }
    void setUndockedHeight(int newUndockedHeight) { m_undockedHeight = newUndockedHeight; }

    int undockedX() const { return m_undockedX; }
    void setUndockedX(int newUndockedX) { m_undockedX = newUndockedX; }

    int undockedY() const { return m_undockedY; }
    void setUndockedY(int newUndockedY) { m_undockedY = newUndockedY; }

private:
    const NavigationModel::NavigationId m_id;
    QString m_shortLabel;
    QString m_longLabel;
    QString m_iconName;
    QString m_qmlComponentPath;
    QFlags<NavigationModel::NavigationOption> m_options;
    int m_undockedWidth = -1;   // 600;
    int m_undockedHeight = -1;  // 400;
    int m_undockedX = -1;
    int m_undockedY = -1;
};

#endif  // NAVIGATIONMODEL_H
