#include "StdInc.h"
#include "cmodmanager.h"

#include "../../lib/VCMIDirs.h"
#include "../../lib/filesystem/Filesystem.h"
#include "../../lib/filesystem/CZipLoader.h"

#include "../jsonutils.h"
#include "../launcherdirs.h"

static QString detectModArchive(QString path, QString modName)
{
	auto files = ZipArchive::listFiles(path.toUtf8().data());

	QString modDirName;

	for (auto file : files)
	{
		QString filename = QString::fromUtf8(file.c_str());
		if (filename.toLower().startsWith(modName))
		{
			// archive must contain mod.json file
			if (filename.toLower() == modName + "/mod.json")
				modDirName = filename.section('/', 0, 0);
		}
		else // all files must be in <modname> directory
			return "";
	}
	return modDirName;
}

CModManager::CModManager(CModList * modList):
    modList(modList)
{
	loadMods();
	loadModSettings();
}

QString CModManager::settingsPath()
{
	return QString::fromUtf8(VCMIDirs::get().userConfigPath().c_str()) + "/modSettings.json";
}

void CModManager::loadModSettings()
{
	modSettings = JsonUtils::JsonFromFile(settingsPath()).toMap();
	modList->setModSettings(modSettings["activeMods"]);
}

void CModManager::resetRepositories()
{
	modList->resetRepositories();
}

void CModManager::loadRepository(QString file)
{
	modList->addRepository(JsonUtils::JsonFromFile(file).toMap());
}

void CModManager::loadMods()
{
	auto installedMods = CResourceHandler::getAvailableMods();

	for (auto modname : installedMods)
	{
		ResourceID resID("Mods/" + modname + "/mod.json");
		if (CResourceHandler::get()->existsResource(resID))
		{
			std::string name = *CResourceHandler::get()->getResourceName(resID);
			auto mod = JsonUtils::JsonFromFile(QString::fromUtf8(name.c_str()));
			localMods.insert(QString::fromUtf8(modname.c_str()).toLower(), mod);
		}
	}
	modList->setLocalModList(localMods);
}

bool CModManager::addError(QString modname, QString message)
{
	recentErrors.push_back(QString("%1: %2").arg(modname).arg(message));
	return false;
}

QStringList CModManager::getErrors()
{
	QStringList ret = recentErrors;
	recentErrors.clear();
	return ret;
}

bool CModManager::installMod(QString modname, QString archivePath)
{
	return canInstallMod(modname) && doInstallMod(modname, archivePath);
}

bool CModManager::uninstallMod(QString modname)
{
	return canUninstallMod(modname) && doUninstallMod(modname);
}

bool CModManager::enableMod(QString modname)
{
	return canEnableMod(modname) && doEnableMod(modname, true);
}

bool CModManager::disableMod(QString modname)
{
	return canDisableMod(modname) && doEnableMod(modname, false);
}

bool CModManager::canInstallMod(QString modname)
{
	auto mod = modList->getMod(modname);

	if (mod.isInstalled())
		return addError(modname, "Mod is already installed");

	if (!mod.isAvailable())
		return addError(modname, "Mod is not available");
	return true;
}

bool CModManager::canUninstallMod(QString modname)
{
	auto mod = modList->getMod(modname);

	if (!mod.isInstalled())
		return addError(modname, "Mod is not installed");

	if (mod.isEnabled())
		return addError(modname, "Mod must be disabled first");
	return true;
}

bool CModManager::canEnableMod(QString modname)
{
	auto mod = modList->getMod(modname);

	if (mod.isEnabled())
		return addError(modname, "Mod is already enabled");

	if (!mod.isInstalled())
		return addError(modname, "Mod must be installed first");

	for (auto modEntry : mod.getValue("depends").toStringList())
	{
		if (!modList->hasMod(modEntry)) // required mod is not available
			return addError(modname, QString("Required mod %1 is missing").arg(modEntry));
		if (!modList->getMod(modEntry).isEnabled())
			return addError(modname, QString("Required mod %1 is not enabled").arg(modEntry));
	}

	for (QString modEntry : modList->getModList())
	{
		auto mod = modList->getMod(modEntry);

		// "reverse conflict" - enabled mod has this one as conflict
		if (mod.isEnabled() && mod.getValue("conflicts").toStringList().contains(modname))
			return addError(modname, QString("This mod conflicts with %1").arg(modEntry));
	}

	for (auto modEntry : mod.getValue("conflicts").toStringList())
	{
		if (modList->hasMod(modEntry) &&
		    modList->getMod(modEntry).isEnabled()) // conflicting mod installed and enabled
			return addError(modname, QString("This mod conflicts with %1").arg(modEntry));
	}
	return true;
}

bool CModManager::canDisableMod(QString modname)
{
	auto mod = modList->getMod(modname);

	if (mod.isDisabled())
		return addError(modname, "Mod is already disabled");

	if (!mod.isInstalled())
		return addError(modname, "Mod must be installed first");

	for (QString modEntry : modList->getModList())
	{
		auto current = modList->getMod(modEntry);

		if (current.getValue("depends").toStringList().contains(modname) &&
		    current.isEnabled())
			return addError(modname, QString("This mod is needed to run %1").arg(modEntry));
	}
	return true;
}

bool CModManager::doEnableMod(QString mod, bool on)
{
	QVariant value(on);
	QVariantMap list = modSettings["activeMods"].toMap();
	QVariantMap modData = list[mod].toMap();

	modData.insert("active", value);
	list.insert(mod, modData);
	modSettings.insert("activeMods", list);

	modList->setModSettings(modSettings["activeMods"]);

	JsonUtils::JsonToFile(settingsPath(), modSettings);

	return true;
}

bool CModManager::doInstallMod(QString modname, QString archivePath)
{
	QString destDir = CLauncherDirs::get().modsPath() + "/";

	if (!QFile(archivePath).exists())
		return addError(modname, "Mod archive is missing");

	// FIXME: recheck wog/vcmi data behavior - they have bits of data in our trunk
	// FIXME: breaks when there is Era mod with same name
	//if (QDir(destDir + modname).exists())
	//	return addError(modname, "Mod with such name is already installed");

	if (localMods.contains(modname))
		return addError(modname, "Mod with such name is already installed");

	QString modDirName = detectModArchive(archivePath, modname);
	if (!modDirName.size())
		return addError(modname, "Mod archive is invalid or corrupted");

	if (!ZipArchive::extract(archivePath.toUtf8().data(), destDir.toUtf8().data()))
	{
		QDir(destDir + modDirName).removeRecursively();
		return addError(modname, "Failed to extract mod data");
	}

	QVariantMap json = JsonUtils::JsonFromFile(destDir + modDirName + "/mod.json").toMap();

	localMods.insert(modname, json);
	modList->setLocalModList(localMods);

	return true;
}

bool CModManager::doUninstallMod(QString modname)
{
	ResourceID resID(std::string("Mods/") + modname.toUtf8().data(), EResType::DIRECTORY);
	// Get location of the mod, in case-insensitive way
	QString modDir = QString::fromUtf8(CResourceHandler::get()->getResourceName(resID)->c_str());

	if (!QDir(modDir).exists())
		return addError(modname, "Data with this mod was not found");

	if (!localMods.contains(modname))
		return addError(modname, "Data with this mod was not found");

	if (!QDir(modDir).removeRecursively())
		return addError(modname, "Failed to delete mod data");

	localMods.remove(modname);
	modList->setLocalModList(localMods);

	return true;
}
