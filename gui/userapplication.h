#ifndef USERAPPLICATION_H
#define USERAPPLICATION_H

#include <QObject>
#include <QPixmap>
#include <QHash>
#include "radiocontrol.h"
#include "motdecoder.h"

class UserApplication : public QObject
{
    Q_OBJECT
public:
    UserApplication(RadioControl * radioControlPtr, QObject *parent = nullptr);

    virtual void onNewMOTObject(const MOTObject & obj) = 0;
    virtual void onUserAppData(const RadioControlUserAppData & data) = 0;
    virtual void start() = 0;
    virtual void stop() = 0;
    virtual void restart() = 0;    

signals:
    void resetTerminal();

protected:
    bool isRunning;
    RadioControl * radioControl;
};

class Slide
{
public:
    Slide();
    QPixmap getPixmap() const;
    bool setPixmap(const QByteArray &data);

    const QString &getContentName() const;
    void setContentName(const QString &newContentName);

    const QString &getCategoryTitle() const;
    void setCategoryTitle(const QString &newCategoryTitle);

    const QString &getClickThroughURL() const;
    void setClickThroughURL(const QString &newClickThroughURL);

    int getCategoryID() const;
    void setCategoryID(int newCategoryID);

    int getSlideID() const;
    void setSlideID(int newSlideID);

    const QString &getAlternativeLocationURL() const;
    void setAlternativeLocationURL(const QString &newAlternativeLocationURL);

private:
    QPixmap pixmap;
    QString contentName;
    QString categoryTitle;
    QString clickThroughURL;
    QString alternativeLocationURL;
    int categoryID;
    int slideID;
};


class SlideShowApp : public UserApplication
{
    Q_OBJECT
    enum class Parameter
    {
        ExpireTime = 0x04,
        TriggerTime = 0x05,
        ContentName = 0x0C,
        CategoryID = 0x25,
        CategoryTitle = 0x26,
        ClickThroughURL = 0x27,
        AlternativeLocationURL = 0x28,
        Alert = 0x29
    };

    class Category
    {
    public:
        Category(QString & categoryTitle);
        const QString &getTitle() const;
        void setTitle(const QString &newTitle);
        void insertSlide(Slide * s);
        bool removeSlide(int id);
        Slide * getSlide(int id);
        Slide * getNextSlide(bool moveForward = true);
    private:
        int currentSlide;
        QString title;
        QMap<int, Slide*> slides;
    };

public:
    SlideShowApp(RadioControl * radioControlPtr, QObject *parent = nullptr);
    ~SlideShowApp();
    void onNewMOTObject(const MOTObject & obj) override;
    void onUserAppData(const RadioControlUserAppData & data) override;
    void start() override;
    void stop() override;
    void restart() override;

signals:
    void newSlide(const Slide & slide);

private:
    MOTDecoder * decoder;
    QHash<QString, Slide> cache;
    QHash<int, Category> catSls;
};

#endif // USERAPPLICATION_H
