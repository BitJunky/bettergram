#pragma once

#include <QObject>

namespace Bettergram {

class RssChannel;
class RssItem;

/**
 * @brief The RssChannelList class contains list of RssChannel instances.
 */
class RssChannelList : public QObject {
	Q_OBJECT

public:
	typedef QList<QSharedPointer<RssChannel>>::const_iterator const_iterator;

	explicit RssChannelList(const QString &name,
							int imageWidth,
							int imageHeight,
							QObject *parent = nullptr);

	int freq() const;
	void setFreq(int freq);

	QDateTime lastUpdate() const;
	QString lastUpdateString() const;

	const_iterator begin() const;
	const_iterator end() const;

	/// Can throw std::out_of_range() exception
	const QSharedPointer<RssChannel> &at(int index) const;

	bool isEmpty() const;
	int count() const;
	int countAllItems() const;
	int countAllUnreadItems() const;

	void add(const QUrl &channelLink);

	void markAsRead();

	QList<QSharedPointer<RssItem>> getAllItems() const;
	QList<QSharedPointer<RssItem>> getAllUnreadItems() const;

	void load();
	void parse();

public slots:

signals:
	void update();

	void freqChanged();
	void lastUpdateChanged();
	void iconChanged();
	
	void updated();

protected:

private:
	/// Default frequency of updates in seconds
	static const int _defaultFreq;

	QList<QSharedPointer<RssChannel>> _list;

	/// It is used for storing and loading data
	const QString _name;

	int _imageWidth = 0;
	int _imageHeight = 0;

	/// Frequency of updates in seconds
	int _freq;

	QDateTime _lastUpdate;
	QString _lastUpdateString;

	void setLastUpdate(const QDateTime &lastUpdate);
	void add(QSharedPointer<RssChannel> &channel);

	void save();

private slots:
	void onIsReadChanged();
};

} // namespace Bettergram
