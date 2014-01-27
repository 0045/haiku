/*
 * Copyright 2008 Ralf Schülke, ralf.schuelke@googlemail.com.
 * Copyright 2014 Haiku, Inc. All rights reserved.
 *
 * Distributed under the terms of the MIT License.
 *
 * Authors:
 *		John Scipione, jscipione@gmail.com
 */
 
#include <stdio.h>
#include <unistd.h>

#include <Button.h>

#include "PairsTopButton.h"
#include "PairsGlobal.h"


TopButton::TopButton(int x, int y, BMessage* message)
	: BButton(BRect(x, y, x + kBitmapSize, y + kBitmapSize), "top_button",
		"?", message)
{
	SetFontSize(54);
}


TopButton::~TopButton()
{
}
