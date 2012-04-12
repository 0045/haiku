/*
 * Copyright 2009-2012 Haiku Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 *
 * Authors:
 *		John Scipione <jscipione@gmail.com>
 *		Alex Wilson <yourpalal2@gmail.com>
 *		Artur Wyszynski <harakash@gmail.com>
 */


#include "ExtensionsView.h"

#include <Catalog.h>
#include <GL/gl.h>
#include <GL/glu.h>
#include <GroupLayout.h>
#include <GroupLayoutBuilder.h>
#include <Locale.h>
#include <Message.h>
#include <SpaceLayoutItem.h>
#include <String.h>


#undef B_TRANSLATE_CONTEXT
#define B_TRANSLATE_CONTEXT "Extensions"


ExtensionsView::ExtensionsView()
	:
	BGroupView(B_TRANSLATE("Extensions"), B_VERTICAL),
	fExtensionsList(new BColumnListView("ExtensionsList", B_FOLLOW_ALL))
{
	// add the columns

	fAvailableColumn = new BStringColumn(B_TRANSLATE("Available extensions"),
		280, 280, 280, B_TRUNCATE_MIDDLE);
	fExtensionsList->AddColumn(fAvailableColumn, 0);
	fExtensionsList->SetSortingEnabled(true);
	fExtensionsList->SetSortColumn(fAvailableColumn, true, true);

	// add the rows

	_AddExtensionsList(fExtensionsList, (char*)glGetString(GL_EXTENSIONS));
	_AddExtensionsList(fExtensionsList, (char*)gluGetString(GLU_EXTENSIONS));

	// add the list

	AddChild(fExtensionsList);
	GroupLayout()->SetInsets(B_USE_DEFAULT_SPACING, B_USE_DEFAULT_SPACING,
		B_USE_DEFAULT_SPACING, B_USE_DEFAULT_SPACING);
}


ExtensionsView::~ExtensionsView()
{
	BRow *row;
	while ((row = fExtensionsList->RowAt((int32)0, NULL)) != NULL) {
		fExtensionsList->RemoveRow(row);
		delete row;
	}
	delete fAvailableColumn;
	delete fExtensionsList;
}


//	#pragma mark -


void
ExtensionsView::_AddExtensionsList(BColumnListView* fExtensionsList, char* stringList)
{
	if (stringList == NULL)
		// empty extentions string
		return;

	while (*stringList != NULL) {
		char extName[255];
		int n = strcspn(stringList, " ");
		strncpy(extName, stringList, n);
		extName[n] = 0;
		BRow* row = new BRow();
		row->SetField(new BStringField(extName), 0);
		fExtensionsList->AddRow(row);
		if (!stringList[n])
			break;
		stringList += (n + 1);
			// next !
	}
}
