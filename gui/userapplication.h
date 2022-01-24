#ifndef USERAPPLICATION_H
#define USERAPPLICATION_H

#include <QObject>
#include <QPixmap>
#include <QHash>
#include <QSharedData>
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

class SlideData : public QSharedData
{
public:
    SlideData();
    SlideData(const SlideData & other);
    ~SlideData() {}

    QPixmap pixmap;
    QString contentName;
    QString categoryTitle;
    QString clickThroughURL;
    QString alternativeLocationURL;
    int categoryID;
    int slideID;
};

class Slide
{
public:
    Slide();    
    Slide(const Slide &other) : d (other.d) { }

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

    bool isDecategorizeRequested() const;
    bool isEmpty() const { return d->contentName.isEmpty(); }

    bool operator==(const Slide & other) const;

private:
    QSharedDataPointer<SlideData> d;
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

    class Category;

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
    void newCategory(int id, const QString & title);
    void categoryRemoved(int id);

private:
    MOTDecoder * decoder;
    QHash<QString, Slide> cache;   
    QHash<int, Category> catSls;

    void addSlideToCategory(const Slide & slide);
    void removeSlideFromCategory(const Slide & slide);
};

class SlideShowApp::Category
{
public:
    Category(QString & categoryTitle);
    const QString &getTitle() const;
    void setTitle(const QString &newTitle);
    void insertSlide(const Slide & s);
    void removeSlide(int id);
    int size() const { return slides.size(); }
    QString getFirstSlide();
    QString getNextSlide(bool moveForward = true);

private:
    int currentSlide;
    QString title;
    QMap<int, QString> slides;
};



#endif // USERAPPLICATION_H
