/*
 * Copyright 2009, Ingo Weinhold, ingo_weinhold@gmx.de.
 * Copyright 2011, Rene Gollent, rene@gollent.com.
 * Distributed under the terms of the MIT License.
 */

#include "DwarfUtils.h"

#include <String.h>

#include "CompilationUnit.h"
#include "Dwarf.h"
#include "DwarfFile.h"


/*static*/ void
DwarfUtils::GetDIEName(const DebugInfoEntry* entry, BString& _name)
{
	// If we don't seem to have a name but an abstract origin, return the
	// origin's name.
	const char* name = entry->Name();
	if (name == NULL) {
		if (DebugInfoEntry* abstractOrigin = entry->AbstractOrigin()) {
			entry = abstractOrigin;
			name = entry->Name();
		}
	}

	// If we still don't have a name but a specification, return the
	// specification's name.
	if (name == NULL) {
		if (DebugInfoEntry* specification = entry->Specification()) {
			entry = specification;
			name = entry->Name();
		}
	}

	_name = name;
}


/*static*/ void
DwarfUtils::GetFullDIEName(const DebugInfoEntry* entry, BString& _name)
{
	BString generatedName;
	// If we don't seem to have a name but an abstract origin, return the
	// origin's name.
	const char* name = entry->Name();
	if (name == NULL) {
		if (DebugInfoEntry* abstractOrigin = entry->AbstractOrigin()) {
			entry = abstractOrigin;
			name = entry->Name();
		}
	}

	// If we still don't have a name but a specification, return the
	// specification's name.
	if (name == NULL) {
		if (DebugInfoEntry* specification = entry->Specification()) {
			entry = specification;
			name = entry->Name();
		}
	}

	// we found no name for this entry whatsoever, abort.
	if (name == NULL)
		return;

	generatedName = name;

	const DIESubprogram* subProgram = dynamic_cast<const DIESubprogram*>(
		entry);
	if (subProgram != NULL) {
		// TODO: retrieve template parameters
		generatedName += "(";
		BString parameters;
		DebugInfoEntryList::ConstIterator iterator
			= subProgram->Parameters().GetIterator();

		// this function is a class method, skip the first parameter
		// as it supplies our 'this' pointer and shouldn't be visible
		// in the signature
		if (dynamic_cast<const DIECompoundType*>(subProgram->Parent()) != NULL)
			iterator.Next();

		while (iterator.HasNext()) {
			const DIEFormalParameter* parameter
				= dynamic_cast<DIEFormalParameter*>(iterator.Next());
			if (parameter == NULL) {
				// this shouldn't happen
				return;
			}

			BString paramName;
			BString modifier;
			DIEType* type = parameter->GetType();
			if (DIEModifiedType* modifiedType = dynamic_cast<DIEModifiedType*>(
				type)) {
				DIEType* baseType = type;
				while ((modifiedType = dynamic_cast<DIEModifiedType*>(
					baseType)) != NULL && modifiedType->GetType() != NULL) {
					switch (modifiedType->Tag()) {
						case DW_TAG_pointer_type:
							modifier += "*";
							break;
						case DW_TAG_reference_type:
							modifier += "&";
							break;
						case DW_TAG_const_type:
							modifier += " const ";
							break;
						default:
							break;
					}

					baseType = modifiedType->GetType();
				}
				type = baseType;
			}

			GetFullyQualifiedDIEName(type, paramName);
			parameters += paramName;
			if (modifier.Length() > 0) {
				if (modifier[modifier.Length() - 1] == ' ')
					modifier.Truncate(modifier.Length() - 1);
				parameters += modifier;
			}

			if (iterator.HasNext())
				parameters += ", ";
		}

		if (parameters.Length() > 0)
			generatedName += parameters;
		else
			generatedName += "void";
		generatedName += ")";
	}

	_name = generatedName;
}


/*static*/ void
DwarfUtils::GetFullyQualifiedDIEName(const DebugInfoEntry* entry,
	BString& _name)
{
	// If we don't seem to have a name but an abstract origin, return the
	// origin's name.
	if (entry->Name() == NULL) {
		if (DebugInfoEntry* abstractOrigin = entry->AbstractOrigin())
			entry = abstractOrigin;
	}

	// If we don't still don't have a name but a specification, get the
	// specification's name.
	if (entry->Name() == NULL) {
		if (DebugInfoEntry* specification = entry->Specification())
			entry = specification;
	}

	_name.Truncate(0);

	// Get the namespace, if any.
	DebugInfoEntry* parent = entry->Parent();
	while (parent != NULL) {
		if (parent->IsNamespace()) {
			GetFullyQualifiedDIEName(parent, _name);
			break;
		}

		parent = parent->Parent();
	}

	BString name;
	GetFullDIEName(entry, name);

	if (name.Length() == 0)
		return;

	if (_name.Length() > 0) {
		_name << "::" << name;
	} else
		_name = name;
}


/*static*/ bool
DwarfUtils::GetDeclarationLocation(DwarfFile* dwarfFile,
	const DebugInfoEntry* entry, const char*& _directory, const char*& _file,
	int32& _line, int32& _column)
{
	uint32 file = 0;
	uint32 line = 0;
	uint32 column = 0;
	bool fileSet = entry->GetDeclarationFile(file);
	bool lineSet = entry->GetDeclarationLine(line);
	bool columnSet = entry->GetDeclarationColumn(column);

	// if something is not set yet, try the abstract origin (if any)
	if (!fileSet || !lineSet || !columnSet) {
		if (DebugInfoEntry* abstractOrigin = entry->AbstractOrigin()) {
			entry = abstractOrigin;
			if (!fileSet)
				fileSet = entry->GetDeclarationFile(file);
			if (!lineSet)
				lineSet = entry->GetDeclarationLine(line);
			if (!columnSet)
				columnSet = entry->GetDeclarationColumn(column);
		}
	}

	// something is not set yet, try the specification (if any)
	if (!fileSet || !lineSet || !columnSet) {
		if (DebugInfoEntry* specification = entry->Specification()) {
			entry = specification;
			if (!fileSet)
				fileSet = entry->GetDeclarationFile(file);
			if (!lineSet)
				lineSet = entry->GetDeclarationLine(line);
			if (!columnSet)
				columnSet = entry->GetDeclarationColumn(column);
		}
	}

	if (file == 0)
		return false;

	// get the compilation unit
	CompilationUnit* unit = dwarfFile->CompilationUnitForDIE(entry);
	if (unit == NULL)
		return false;

	const char* directoryName;
	const char* fileName = unit->FileAt(file - 1, &directoryName);
	if (fileName == NULL)
		return false;

	_directory = directoryName;
	_file = fileName;
	_line = (int32)line - 1;
	_column = (int32)column - 1;
	return true;
}
