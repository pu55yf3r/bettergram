#include "resourcegrouplist.h"
#include "resourcegroup.h"

#include <bettergram/bettergramservice.h>
#include <logs.h>

#include <QJsonDocument>

namespace Bettergram {

ResourceGroupList::ResourceGroupList(QObject *parent) :
	QObject(parent),
	_freq(_defaultFreq),
	_lastUpdateString(BettergramService::defaultLastUpdateString())
{
}

int ResourceGroupList::freq() const
{
	return _freq;
}

void ResourceGroupList::setFreq(int freq)
{
	if (freq <= 0) {
		freq = _defaultFreq;
	}

	if (_freq != freq) {
		_freq = freq;
		emit freqChanged();
	}
}

QDateTime ResourceGroupList::lastUpdate() const
{
	return _lastUpdate;
}

QString ResourceGroupList::lastUpdateString() const
{
	return _lastUpdateString;
}

void ResourceGroupList::setLastUpdate(const QDateTime &lastUpdate)
{
	if (_lastUpdate != lastUpdate) {
		_lastUpdate = lastUpdate;

		_lastUpdateString = BettergramService::generateLastUpdateString(_lastUpdate, true);
		emit lastUpdateChanged();
	}
}

ResourceGroupList::const_iterator ResourceGroupList::begin() const
{
	return _list.begin();
}

ResourceGroupList::const_iterator ResourceGroupList::end() const
{
	return _list.end();
}

const QSharedPointer<ResourceGroup> &ResourceGroupList::at(int index) const
{
	if (index < 0 || index >= _list.size()) {
		LOG(("Index is out of bounds"));
		throw std::out_of_range("resource group index is out of range");
	}

	return _list.at(index);
}

bool ResourceGroupList::isEmpty() const
{
	return _list.isEmpty();
}

int ResourceGroupList::count() const
{
	return _list.count();
}

bool ResourceGroupList::parseFile(const QString &filePath)
{
	QFile file(filePath);

	if (!file.open(QIODevice::ReadOnly)) {
		LOG(("Unable to open file '%1' with resource group list").arg(filePath));
		return false;
	}

	return parse(file.readAll());
}

bool ResourceGroupList::parse(const QByteArray &byteArray)
{
	// Update only if it has been changed
	QByteArray hash = QCryptographicHash::hash(byteArray, QCryptographicHash::Sha256);

	if (hash == _lastSourceHash) {
		setLastUpdate(QDateTime::currentDateTime());
		return false;
	}

	QJsonParseError parseError;
	QJsonDocument doc = QJsonDocument::fromJson(byteArray, &parseError);

	if (!doc.isObject()) {
		LOG(("Can not get resource group list. Data is wrong. %1 (%2). Data: %3")
			.arg(parseError.errorString())
			.arg(parseError.error)
			.arg(QString::fromUtf8(byteArray)));

		return false;
	}

	QJsonObject json = doc.object();

	if (json.isEmpty()) {
		LOG(("Can not get resource group list. Data is emtpy or wrong"));
		return false;
	}

	if (!parse(json)) {
		return false;
	}

	save(byteArray);

	_lastSourceHash = hash;

	return true;
}

bool ResourceGroupList::parse(const QJsonObject &json)
{
	if (json.isEmpty()) {
		return false;
	}

	if (json.contains("freq")) {
		setFreq(json.value("freq").toInt());
	}

	QJsonArray groupsJson;

	if (json.contains("resources")) {
		if (!json.value("success").toBool()) {
			return false;
		}

		groupsJson = json.value("resources").toObject().value("groups").toArray();
	} else {
		groupsJson = json.value("groups").toArray();
	}

	_list.clear();

	for (const QJsonValue value : groupsJson) {
		if (!value.isObject()) {
			LOG(("Unable to get json object for resource group"));
			continue;
		}

		QSharedPointer<ResourceGroup> group(new ResourceGroup());

		connect(group.data(), &ResourceGroup::iconChanged, this, &ResourceGroupList::iconChanged);

		group->parse(value.toObject());
		_list.push_back(group);
	}

	setLastUpdate(QDateTime::currentDateTime());
	emit updated();

	return true;
}

void ResourceGroupList::save(const QByteArray &byteArray)
{
	const QString filePath = BettergramService::instance()->resourcesCachePath();

	QFile file(filePath);

	if (!file.open(QIODevice::WriteOnly)) {
		LOG(("Unable to open file '%1' for writing").arg(filePath));
		return;
	}

	int count = file.write(byteArray);

	if (count != byteArray.size()) {
		LOG(("Unable to write all data to file '%1'").arg(filePath));
	}

	file.close();
}

} // namespace Bettergrams
