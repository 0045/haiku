/*
 * Copyright 2010, Stephan Aßmus <superstippi@gmx.de>.
 * Distributed under the terms of the MIT License.
 */
#ifndef SUBTITLE_BITMAP_H
#define SUBTITLE_BITMAP_H


#include <Rect.h>
#include <String.h>


class BBitmap;
class BTextView;


class SubtitleBitmap {
public:
								SubtitleBitmap();
	virtual						~SubtitleBitmap();

			void				SetText(const char* text);
			void				SetVideoBounds(BRect bounds);
			void				SetOverlayMode(bool overlayMode);

			const BBitmap*		Bitmap() const;

private:
			void				_GenerateBitmap();
			void				_InsertText(BRect& bounds,
									float& outlineRadius, bool overlayMode);

private:
			BBitmap*			fBitmap;
			BTextView*			fTextView;
			BTextView*			fShadowTextView;
			BString				fText;

			BRect				fVideoBounds;
			bool				fUseSoftShadow;
			bool				fOverlayMode;
};


#endif	// SUBTITLE_BITMAP_H
