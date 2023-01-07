#ifndef SLIDESHOWAPP_H
#define SLIDESHOWAPP_H

#include <QObject>
#include <QPixmap>
#include <QHash>
#include <QSharedData>
#include "radiocontrol.h"
#include "motdecoder.h"
#include "userapplication.h"


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
    SlideShowApp(QObject *parent = nullptr);
    ~SlideShowApp();
    void onNewMOTObject(const MOTObject & obj) override;
    void onUserAppData(const RadioControlUserAppData & data) override;
    void start() override;
    void stop() override;
    void restart() override;

    void getCurrentCatSlide(int catId);
    void getNextCatSlide(int catId, bool forward = true);
signals:
    // this signal is emitted anytime when new slide is received
    void currentSlide(const Slide & slide);

    // catSLS signals
    void categoryUpdate(int catId, const QString & title);
    void catSlide(const Slide & slide, int catId, int slideIdx, int numSlides);

    void catSlsAvailable(bool isAvailable);
private:
    MOTDecoder * m_decoder;
    QHash<QString, Slide> m_cache;
    QHash<int, Category> m_catSls;

    void addSlideToCategory(const Slide & slide);
    void removeSlideFromCategory(const Slide & slide);
};

class SlideShowApp::Category
{
public:
    Category(QString & categoryTitle);
    const QString &getTitle() const;
    void setTitle(const QString &newTitle);
    int insertSlide(const Slide & s);
    int removeSlide(int id);
    int size() const;
    QString getCurrentSlide() const;
    QString getNextSlide(bool moveForward = true);
    int getCurrentIndex() const;
private:
    struct SlideItem
    {
        int id;
        QString contentName;
    };

    int m_currentSlide;
    int m_currentSlideIdx;
    QString m_title;
    QList<SlideItem> m_slidesList;
};

#endif // SLIDESHOWAPP_H
