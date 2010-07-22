/*
 * Copyright 2003-2009, Axel Dörfler, axeld@pinc-software.de.
 * Copyright 2009, Destugues, pulkomandy@gmail.com.
 * Distributed under the terms of the MIT License.
 */


#include <Country.h>

#include <assert.h>
#include <iostream>
#include <stdlib.h>
#include <vector>

#include <CalendarView.h>
#include <IconUtils.h>
#include <Resources.h>
#include <String.h>

#include <unicode/datefmt.h>
#include <unicode/dcfmtsym.h>
#include <unicode/decimfmt.h>
#include <unicode/dtfmtsym.h>
#include <unicode/smpdtfmt.h>
#include <ICUWrapper.h>

#include <monetary.h>
#include <stdarg.h>


#define ICU_VERSION icu_44


using BPrivate::B_WEEK_START_MONDAY;
using BPrivate::B_WEEK_START_SUNDAY;


BCountry::BCountry(const char* languageCode, const char* countryCode)
{
	fICULocale = new ICU_VERSION::Locale(languageCode, countryCode);
	fICULongDateFormatter = DateFormat::createDateInstance(
		DateFormat::FULL, *fICULocale);
 	fICUShortDateFormatter = DateFormat::createDateInstance(
		DateFormat::SHORT, *fICULocale);
	fICULongTimeFormatter = DateFormat::createTimeInstance(
		DateFormat::MEDIUM, *fICULocale);
 	fICUShortTimeFormatter = DateFormat::createTimeInstance(
		DateFormat::SHORT, *fICULocale);
}


BCountry::BCountry(const char* languageAndCountryCode)
{
	fICULocale = new ICU_VERSION::Locale(languageAndCountryCode);
	fICULongDateFormatter = DateFormat::createDateInstance(
		DateFormat::FULL, *fICULocale);
 	fICUShortDateFormatter = DateFormat::createDateInstance(
		DateFormat::SHORT, *fICULocale);
	fICULongTimeFormatter = DateFormat::createTimeInstance(
		DateFormat::MEDIUM, *fICULocale);
 	fICUShortTimeFormatter = DateFormat::createTimeInstance(
		DateFormat::SHORT, *fICULocale);
}


BCountry::BCountry(const BCountry& other)
{
	fICULocale = new ICU_VERSION::Locale(*other.fICULocale);
	fICULongDateFormatter = new ICU_VERSION::SimpleDateFormat(
		*static_cast<SimpleDateFormat*>(other.fICULongDateFormatter));
 	fICUShortDateFormatter = new ICU_VERSION::SimpleDateFormat(
		*static_cast<SimpleDateFormat*>(other.fICUShortDateFormatter));
	fICULongTimeFormatter = new ICU_VERSION::SimpleDateFormat(
		*static_cast<SimpleDateFormat*>(other.fICULongTimeFormatter));
 	fICUShortTimeFormatter = new ICU_VERSION::SimpleDateFormat(
		*static_cast<SimpleDateFormat*>(other.fICUShortTimeFormatter));
}


BCountry&
BCountry::operator=(const BCountry& other)
{
	if (this == &other)
		return *this;

	delete fICULongTimeFormatter;
	delete fICUShortTimeFormatter;
	delete fICULongDateFormatter;
	delete fICUShortDateFormatter;
	delete fICULocale;

	fICULocale = new ICU_VERSION::Locale(*other.fICULocale);
	fICULongDateFormatter = new ICU_VERSION::SimpleDateFormat(
		*static_cast<SimpleDateFormat*>(other.fICULongDateFormatter));
 	fICUShortDateFormatter = new ICU_VERSION::SimpleDateFormat(
		*static_cast<SimpleDateFormat*>(other.fICUShortDateFormatter));
	fICULongTimeFormatter = new ICU_VERSION::SimpleDateFormat(
		*static_cast<SimpleDateFormat*>(other.fICULongTimeFormatter));
 	fICUShortTimeFormatter = new ICU_VERSION::SimpleDateFormat(
		*static_cast<SimpleDateFormat*>(other.fICUShortTimeFormatter));

	return *this;
}


BCountry::~BCountry()
{
	delete fICULongTimeFormatter;
	delete fICUShortTimeFormatter;
	delete fICULongDateFormatter;
	delete fICUShortDateFormatter;
	delete fICULocale;
}


bool
BCountry::Name(BString& name) const
{
	UnicodeString uString;
	fICULocale->getDisplayCountry(uString);
	BStringByteSink stringConverter(&name);
	uString.toUTF8(stringConverter);
	return true;
}


bool
BCountry::LocaleName(BString& name) const
{
	UnicodeString uString;
	fICULocale->getDisplayName(uString);
	BStringByteSink stringConverter(&name);
	uString.toUTF8(stringConverter);
	return true;
}


const char*
BCountry::Code() const
{
	return fICULocale->getName();
}


status_t
BCountry::GetIcon(BBitmap* result)
{
	if (result == NULL)
		return B_BAD_DATA;
	// TODO: a proper way to locate the library being used ?
	BResources storage("/boot/system/lib/liblocale.so");
	if (storage.InitCheck() != B_OK)
		return B_ERROR;
	size_t size;
	const char* code = fICULocale->getCountry();
	if (code != NULL) {
		const void* buffer = storage.LoadResource(B_VECTOR_ICON_TYPE, code,
			&size);
		if (buffer != NULL && size != 0) {
			return BIconUtils::GetVectorIcon(static_cast<const uint8*>(buffer),
				size, result);
		}
	}
	return B_BAD_DATA;
}


void
BCountry::FormatDate(char* string, size_t maxSize, time_t time, bool longFormat)
{
	BString fullString;
	FormatDate(&fullString, time, longFormat);
	strncpy(string, fullString.String(), maxSize);
}


void
BCountry::FormatDate(BString *string, time_t time, bool longFormat)
{
	// TODO: ICU allows for 4 different levels of expansion :
	// short, medium, long, and full. Our bool parameter is not enough...
	ICU_VERSION::DateFormat* dateFormatter
		= longFormat ? fICULongDateFormatter : fICUShortDateFormatter;
	UnicodeString ICUString;
	ICUString = dateFormatter->format((UDate)time * 1000, ICUString);

	string->Truncate(0);
	BStringByteSink stringConverter(string);

	ICUString.toUTF8(stringConverter);
}


status_t
BCountry::FormatDate(BString* string, int*& fieldPositions, int& fieldCount,
	time_t time, bool longFormat)
{
	fieldPositions = NULL;
	UErrorCode error = U_ZERO_ERROR;
	ICU_VERSION::DateFormat* dateFormatter;
	ICU_VERSION::FieldPositionIterator positionIterator;
	dateFormatter = longFormat ? fICULongDateFormatter : fICUShortDateFormatter;
	UnicodeString ICUString;
	ICUString = dateFormatter->format((UDate)time * 1000, ICUString,
		&positionIterator, error);

	if (error != U_ZERO_ERROR)
		return B_ERROR;

	ICU_VERSION::FieldPosition field;
	std::vector<int> fieldPosStorage;
	fieldCount  = 0;
	while (positionIterator.next(field)) {
		fieldPosStorage.push_back(field.getBeginIndex());
		fieldPosStorage.push_back(field.getEndIndex());
		fieldCount += 2;
	}

	fieldPositions = (int*) malloc(fieldCount * sizeof(int));

	for (int i = 0 ; i < fieldCount ; i++ )
		fieldPositions[i] = fieldPosStorage[i];

	string->Truncate(0);
	BStringByteSink stringConverter(string);

	ICUString.toUTF8(stringConverter);

	return B_OK;
}


void
BCountry::FormatTime(char* string, size_t maxSize, time_t time, bool longFormat)
{
	BString fullString;
	FormatTime(&fullString, time, longFormat);
	strncpy(string, fullString.String(), maxSize);
}


void
BCountry::FormatTime(BString* string, time_t time, bool longFormat)
{
	// TODO: ICU allows for 4 different levels of expansion :
	// short, medium, long, and full. Our bool parameter is not enough...
	ICU_VERSION::DateFormat* timeFormatter;
	timeFormatter = longFormat ? fICULongTimeFormatter : fICUShortTimeFormatter;
	UnicodeString ICUString;
	ICUString = timeFormatter->format((UDate)time * 1000, ICUString);

	string->Truncate(0);
	BStringByteSink stringConverter(string);

	ICUString.toUTF8(stringConverter);
}


status_t
BCountry::FormatTime(BString* string, int*& fieldPositions, int& fieldCount,
	time_t time, bool longFormat)
{
	fieldPositions = NULL;
	UErrorCode error = U_ZERO_ERROR;
	ICU_VERSION::DateFormat* timeFormatter;
	ICU_VERSION::FieldPositionIterator positionIterator;
	timeFormatter = longFormat ? fICULongTimeFormatter : fICUShortTimeFormatter;
	UnicodeString ICUString;
	ICUString = timeFormatter->format((UDate)time * 1000, ICUString,
		&positionIterator, error);

	if (error != U_ZERO_ERROR)
		return B_ERROR;

	ICU_VERSION::FieldPosition field;
	std::vector<int> fieldPosStorage;
	fieldCount  = 0;
	while (positionIterator.next(field)) {
		fieldPosStorage.push_back(field.getBeginIndex());
		fieldPosStorage.push_back(field.getEndIndex());
		fieldCount += 2;
	}

	fieldPositions = (int*) malloc(fieldCount * sizeof(int));

	for (int i = 0 ; i < fieldCount ; i++ )
		fieldPositions[i] = fieldPosStorage[i];

	string->Truncate(0);
	BStringByteSink stringConverter(string);

	ICUString.toUTF8(stringConverter);

	return B_OK;
}


status_t
BCountry::TimeFields(BDateElement*& fields, int& fieldCount, bool longFormat)
{
	fields = NULL;
	UErrorCode error = U_ZERO_ERROR;
	ICU_VERSION::DateFormat* timeFormatter;
	ICU_VERSION::FieldPositionIterator positionIterator;
	timeFormatter = longFormat ? fICULongTimeFormatter : fICUShortTimeFormatter;
	UnicodeString ICUString;
	time_t now;
	ICUString = timeFormatter->format((UDate)time(&now) * 1000, ICUString,
		&positionIterator, error);

	if (error != U_ZERO_ERROR)
		return B_ERROR;

	ICU_VERSION::FieldPosition field;
	std::vector<int> fieldPosStorage;
	fieldCount  = 0;
	while (positionIterator.next(field)) {
		fieldPosStorage.push_back(field.getField());
		fieldCount ++;
	}

	fields = (BDateElement*) malloc(fieldCount * sizeof(BDateElement));

	for (int i = 0 ; i < fieldCount ; i++ ) {
		switch (fieldPosStorage[i]) {
			case UDAT_HOUR_OF_DAY1_FIELD:
			case UDAT_HOUR_OF_DAY0_FIELD:
			case UDAT_HOUR1_FIELD:
			case UDAT_HOUR0_FIELD:
				fields[i] = B_DATE_ELEMENT_HOUR;
				break;
			case UDAT_MINUTE_FIELD:
				fields[i] = B_DATE_ELEMENT_MINUTE;
				break;
			case UDAT_SECOND_FIELD:
				fields[i] = B_DATE_ELEMENT_SECOND;
				break;
			case UDAT_AM_PM_FIELD:
				fields[i] = B_DATE_ELEMENT_AM_PM;
				break;
			default:
				fields[i] = B_DATE_ELEMENT_INVALID;
				break;
		}
	}

	return B_OK;
}


status_t
BCountry::DateFields(BDateElement*& fields, int& fieldCount, bool longFormat)
{
	fields = NULL;
	UErrorCode error = U_ZERO_ERROR;
	ICU_VERSION::DateFormat* dateFormatter;
	ICU_VERSION::FieldPositionIterator positionIterator;
	dateFormatter = longFormat ? fICULongDateFormatter : fICUShortDateFormatter;
	UnicodeString ICUString;
	time_t now;
	ICUString = dateFormatter->format((UDate)time(&now) * 1000, ICUString,
		&positionIterator, error);

	if (error != U_ZERO_ERROR)
		return B_ERROR;

	ICU_VERSION::FieldPosition field;
	std::vector<int> fieldPosStorage;
	fieldCount  = 0;
	while (positionIterator.next(field)) {
		fieldPosStorage.push_back(field.getField());
		fieldCount ++;
	}

	fields = (BDateElement*) malloc(fieldCount * sizeof(BDateElement));

	for (int i = 0 ; i < fieldCount ; i++ ) {
		switch (fieldPosStorage[i]) {
			case UDAT_YEAR_FIELD:
				fields[i] = B_DATE_ELEMENT_YEAR;
				break;
			case UDAT_MONTH_FIELD:
				fields[i] = B_DATE_ELEMENT_MONTH;
				break;
			case UDAT_DATE_FIELD:
				fields[i] = B_DATE_ELEMENT_DAY;
				break;
			default:
				fields[i] = B_DATE_ELEMENT_INVALID;
				break;
		}
	}

	return B_OK;
}


bool
BCountry::DateFormat(BString& format, bool longFormat) const
{
	ICU_VERSION::DateFormat* dateFormatter
		= longFormat ? fICULongDateFormatter : fICUShortDateFormatter;
	SimpleDateFormat* dateFormatterImpl
		= static_cast<SimpleDateFormat*>(dateFormatter);

	UnicodeString ICUString;
	ICUString = dateFormatterImpl->toPattern(ICUString);

	BStringByteSink stringConverter(&format);

	ICUString.toUTF8(stringConverter);

	return true;
}


void
BCountry::SetDateFormat(const char* formatString, bool longFormat)
{
	ICU_VERSION::DateFormat* dateFormatter
		= longFormat ? fICULongDateFormatter : fICUShortDateFormatter;
	SimpleDateFormat* dateFormatterImpl
		= static_cast<SimpleDateFormat*>(dateFormatter);

	UnicodeString pattern(formatString);
	dateFormatterImpl->applyPattern(pattern);
}


void
BCountry::SetTimeFormat(const char* formatString, bool longFormat)
{
	ICU_VERSION::DateFormat* dateFormatter
		= longFormat ? fICULongTimeFormatter : fICUShortTimeFormatter;
	SimpleDateFormat* dateFormatterImpl
		= static_cast<SimpleDateFormat*>(dateFormatter);

	UnicodeString pattern(formatString);
	dateFormatterImpl->applyPattern(pattern);
}


bool
BCountry::TimeFormat(BString& format, bool longFormat) const
{
	ICU_VERSION::DateFormat* dateFormatter;
 	dateFormatter = longFormat ? fICULongTimeFormatter : fICUShortTimeFormatter;
	SimpleDateFormat* dateFormatterImpl
		= static_cast<SimpleDateFormat*>(dateFormatter);

	UnicodeString ICUString;
	ICUString = dateFormatterImpl->toPattern(ICUString);

	BStringByteSink stringConverter(&format);

	ICUString.toUTF8(stringConverter);

	return true;
}


int
BCountry::StartOfWeek()
{
	UErrorCode err = U_ZERO_ERROR;
	Calendar* c = Calendar::createInstance(*fICULocale, err);

	if (err == U_ZERO_ERROR && c->getFirstDayOfWeek(err) == UCAL_SUNDAY) {
		delete c;
		return B_WEEK_START_SUNDAY;
	} else {
		delete c;
		// Might be another day, but BeAPI will not handle it
		return B_WEEK_START_MONDAY;
	}
}


void
BCountry::FormatNumber(char* string, size_t maxSize, double value)
{
	BString fullString;
	FormatNumber(&fullString, value);
	strncpy(string, fullString.String(), maxSize);
}


status_t
BCountry::FormatNumber(BString* string, double value)
{
	UErrorCode err = U_ZERO_ERROR;
	NumberFormat* numberFormatter
		= NumberFormat::createInstance(*fICULocale, NumberFormat::kNumberStyle,
			err);

	// Warning: we're returning an ICU error here but the type is status_t.
	if (U_FAILURE(err)) return err;

	UnicodeString ICUString;
	ICUString = numberFormatter->format(value, ICUString);

	string->Truncate(0);
	BStringByteSink stringConverter(string);

	ICUString.toUTF8(stringConverter);

	return U_ZERO_ERROR;
}


void
BCountry::FormatNumber(char* string, size_t maxSize, int32 value)
{
	BString fullString;
	FormatNumber(&fullString, value);
	strncpy(string, fullString.String(), maxSize);
}


void
BCountry::FormatNumber(BString* string, int32 value)
{
	UErrorCode err;
	NumberFormat* numberFormatter
		= NumberFormat::createInstance(*fICULocale, err);

	assert(err == U_ZERO_ERROR);

	UnicodeString ICUString;
	ICUString = numberFormatter->format((int32_t)value, ICUString);

	string->Truncate(0);
	BStringByteSink stringConverter(string);

	ICUString.toUTF8(stringConverter);
}


// This will only work for locales using the decimal system...
bool
BCountry::DecimalPoint(BString& format) const
{
	UErrorCode err;
	NumberFormat* numberFormatter
		= NumberFormat::createInstance(*fICULocale, err);

	assert(err == U_ZERO_ERROR);

	DecimalFormat* decimalFormatter
		= dynamic_cast<DecimalFormat*>(numberFormatter);

	assert(decimalFormatter != NULL);

	const DecimalFormatSymbols* syms
		= decimalFormatter->getDecimalFormatSymbols();

	UnicodeString ICUString;
	ICUString = syms->getSymbol(DecimalFormatSymbols::kDecimalSeparatorSymbol);

	BStringByteSink stringConverter(&format);

	ICUString.toUTF8(stringConverter);

	return true;
}


bool
BCountry::ThousandsSeparator(BString& separator) const
{
	UErrorCode err;
	NumberFormat* numberFormatter
		= NumberFormat::createInstance(*fICULocale, err);
	assert(err == U_ZERO_ERROR);
	DecimalFormat* decimalFormatter
		= dynamic_cast<DecimalFormat*>(numberFormatter);

	assert(decimalFormatter != NULL);

	const DecimalFormatSymbols* syms
		= decimalFormatter->getDecimalFormatSymbols();

	UnicodeString ICUString;
	ICUString = syms->getSymbol(DecimalFormatSymbols::kPatternSeparatorSymbol);

	BStringByteSink stringConverter(&separator);

	ICUString.toUTF8(stringConverter);

	return true;
}


bool
BCountry::Grouping(BString& grouping) const
{
	UErrorCode err;
	NumberFormat* numberFormatter
		= NumberFormat::createInstance(*fICULocale, err);
	assert(err == U_ZERO_ERROR);
	DecimalFormat* decimalFormatter
		= dynamic_cast<DecimalFormat*>(numberFormatter);

	assert(decimalFormatter != NULL);

	const DecimalFormatSymbols* syms
		= decimalFormatter->getDecimalFormatSymbols();

	UnicodeString ICUString;
	ICUString = syms->getSymbol(DecimalFormatSymbols::kGroupingSeparatorSymbol);

	BStringByteSink stringConverter(&grouping);

	ICUString.toUTF8(stringConverter);

	return true;
}


bool
BCountry::PositiveSign(BString& sign) const
{
	UErrorCode err;
	NumberFormat* numberFormatter
		= NumberFormat::createInstance(*fICULocale, err);
	assert(err == U_ZERO_ERROR);
	DecimalFormat* decimalFormatter
		= dynamic_cast<DecimalFormat*>(numberFormatter);

	assert(decimalFormatter != NULL);

	const DecimalFormatSymbols* syms
		= decimalFormatter->getDecimalFormatSymbols();

	UnicodeString ICUString;
	ICUString = syms->getSymbol(DecimalFormatSymbols::kPlusSignSymbol);

	BStringByteSink stringConverter(&sign);

	ICUString.toUTF8(stringConverter);

	return true;
}


bool
BCountry::NegativeSign(BString& sign) const
{
	UErrorCode err;
	NumberFormat* numberFormatter
		= NumberFormat::createInstance(*fICULocale, err);
	assert(err == U_ZERO_ERROR);
	DecimalFormat* decimalFormatter
		= dynamic_cast<DecimalFormat*>(numberFormatter);

	assert(decimalFormatter != NULL);

	const DecimalFormatSymbols* syms
		= decimalFormatter->getDecimalFormatSymbols();

	UnicodeString ICUString;
	ICUString = syms->getSymbol(DecimalFormatSymbols::kMinusSignSymbol);

	BStringByteSink stringConverter(&sign);

	ICUString.toUTF8(stringConverter);

	return true;
}


// TODO does ICU even support this ? Is it in the keywords ?
int8
BCountry::Measurement() const
{
	return B_US;
}


ssize_t
BCountry::FormatMonetary(char* string, size_t maxSize, double value)
{
	BString fullString;
	ssize_t written = FormatMonetary(&fullString, value);
	if (written < 0)
		return written;

	return strlcpy(string, fullString.String(), maxSize);
}


ssize_t
BCountry::FormatMonetary(BString* string, double value)
{
	if (string == NULL)
		return B_BAD_VALUE;

	UErrorCode err;
	NumberFormat* numberFormatter
		= NumberFormat::createCurrencyInstance(*fICULocale, err);

	assert(err == U_ZERO_ERROR);

	UnicodeString ICUString;
	ICUString = numberFormatter->format(value, ICUString);

	string->Truncate(0);
	BStringByteSink stringConverter(string);

	ICUString.toUTF8(stringConverter);

	return string->Length();
}


bool
BCountry::CurrencySymbol(BString& symbol) const
{
	UErrorCode err;
	NumberFormat* numberFormatter
		= NumberFormat::createCurrencyInstance(*fICULocale, err);
	assert(err == U_ZERO_ERROR);
	DecimalFormat* decimalFormatter
		= dynamic_cast<DecimalFormat*>(numberFormatter);

	assert(decimalFormatter != NULL);

	const DecimalFormatSymbols* syms
		= decimalFormatter->getDecimalFormatSymbols();

	UnicodeString ICUString;
	ICUString = syms->getSymbol(DecimalFormatSymbols::kCurrencySymbol);

	BStringByteSink stringConverter(&symbol);

	ICUString.toUTF8(stringConverter);

	return true;
}


bool
BCountry::InternationalCurrencySymbol(BString& symbol) const
{
	UErrorCode err;
	NumberFormat* numberFormatter
		= NumberFormat::createCurrencyInstance(*fICULocale, err);
	assert(err == U_ZERO_ERROR);
	DecimalFormat* decimalFormatter
		= dynamic_cast<DecimalFormat*>(numberFormatter);

	assert(decimalFormatter != NULL);

	const DecimalFormatSymbols* syms
		= decimalFormatter->getDecimalFormatSymbols();

	UnicodeString ICUString;
	ICUString = syms->getSymbol(DecimalFormatSymbols::kIntlCurrencySymbol);

	BStringByteSink stringConverter(&symbol);

	ICUString.toUTF8(stringConverter);

	return true;
}


bool
BCountry::MonDecimalPoint(BString& decimal) const
{
	UErrorCode err;
	NumberFormat* numberFormatter
		= NumberFormat::createCurrencyInstance(*fICULocale, err);
	assert(err == U_ZERO_ERROR);
	DecimalFormat* decimalFormatter
		= dynamic_cast<DecimalFormat*>(numberFormatter);

	assert(decimalFormatter != NULL);

	const DecimalFormatSymbols* syms
		= decimalFormatter->getDecimalFormatSymbols();

	UnicodeString ICUString;
	ICUString = syms->getSymbol(DecimalFormatSymbols::kMonetarySeparatorSymbol);

	BStringByteSink stringConverter(&decimal);

	ICUString.toUTF8(stringConverter);

	return true;
}


bool
BCountry::MonThousandsSeparator(BString& separator) const
{
	UErrorCode err;
	NumberFormat* numberFormatter
		= NumberFormat::createCurrencyInstance(*fICULocale, err);
	assert(err == U_ZERO_ERROR);
	DecimalFormat* decimalFormatter
		= dynamic_cast<DecimalFormat*>(numberFormatter);

	assert(decimalFormatter != NULL);

	const DecimalFormatSymbols* syms
		= decimalFormatter->getDecimalFormatSymbols();

	UnicodeString ICUString;
	ICUString = syms->getSymbol(DecimalFormatSymbols::kPatternSeparatorSymbol);

	BStringByteSink stringConverter(&separator);

	ICUString.toUTF8(stringConverter);

	return true;
}


bool
BCountry::MonGrouping(BString& grouping) const
{
	UErrorCode err;
	NumberFormat* numberFormatter
		= NumberFormat::createCurrencyInstance(*fICULocale, err);
	assert(err == U_ZERO_ERROR);
	DecimalFormat* decimalFormatter
		= dynamic_cast<DecimalFormat*>(numberFormatter);

	assert(decimalFormatter != NULL);

	const DecimalFormatSymbols* syms
		= decimalFormatter->getDecimalFormatSymbols();

	UnicodeString ICUString;
	ICUString = syms->getSymbol(
		DecimalFormatSymbols::kMonetaryGroupingSeparatorSymbol);

	BStringByteSink stringConverter(&grouping);

	ICUString.toUTF8(stringConverter);

	return true;
}


// TODO: is this possible to get from ICU ?
int32
BCountry::MonFracDigits() const
{
	return 2;
}


// #pragma mark - Timezones


status_t
BCountry::GetTimeZones(BMessage* timezones)
{
	if (timezones == NULL)
		return B_BAD_DATA;


	StringEnumeration* icuTimeZoneList = TimeZone::createEnumeration(
		fICULocale->getCountry());
	UErrorCode error = U_ZERO_ERROR;

	const char* tzName;
	while (tzName = icuTimeZoneList->next(NULL, error)) {
		if (error == U_ZERO_ERROR)
			timezones->AddString("zones", tzName);
		else
			error = U_ZERO_ERROR;
	}

	delete icuTimeZoneList;

	return B_OK;
}
