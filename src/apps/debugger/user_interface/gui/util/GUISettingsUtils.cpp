/*
 * Copyright 2011, Rene Gollent, rene@gollent.com.
 * Distributed under the terms of the MIT License.
 */


#include "GUISettingsUtils.h"


#include <SplitView.h>

#include "GUITeamUISettings.h"

/*static*/ status_t
GUISettingsUtils::ArchiveSplitView(const char* sourceName,
	const char* viewName, GUITeamUISettings* settings, BSplitView* view)
{
	BString settingName;

	for (int32 i = 0; i < view->CountItems(); i++) {
		settingName.SetToFormat("%s%sSplit%d", sourceName, viewName, i);
		if (!settings->SetValue(settingName.String(),
			view->ItemWeight(i)))
			return B_NO_MEMORY;

		settingName.SetToFormat("%s%sCollapsed%d", sourceName, viewName, i);
		if (!settings->SetValue(settingName.String(),
			view->IsItemCollapsed(i)))
			return B_NO_MEMORY;
	}

	return B_OK;
}


/*static*/ void
GUISettingsUtils::UnarchiveSplitView(const char* sourceName,
	const char* viewName, const GUITeamUISettings* settings, BSplitView* view)
{
	BString settingName;
	BVariant value;

	for (int32 i = 0; i < view->CountItems(); i++) {
		settingName.SetToFormat("%s%sSplit%d", sourceName, viewName, i);
		status_t error = settings->Value(settingName.String(), value);
		if (error == B_OK) {
			view->SetItemWeight(i, value.ToFloat(),
				i == view->CountItems() - 1);
		}

		settingName.SetToFormat("%s%sCollapsed%d", sourceName, viewName, i);
		error = settings->Value(settingName.String(), value);
		if (error == B_OK)
			view->SetItemCollapsed(i, value.ToBool());
	}
}
