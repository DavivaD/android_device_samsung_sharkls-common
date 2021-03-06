#include <string>
#include <iostream>
#include <expat.h>
#include <stdlib.h>
#include <unistd.h>
#include <utils/Log.h>

#include "SecNativeFeatureCppIf.h"

#include "SecNativeFeatureTagAll.h"

// feature file location (which should be "/data/omc/others.xml")
#define OMC_MPS_FEATURE_XML "/data/omc/others.xml"
// feature file location (which should be "/system/csc/feature.xml")
#define FEATURE_XML "/system/csc/feature.xml"
// feature file location (which should be "/system/csc/others.xml")
#define MPS_FEATURE_XML "/system/csc/others.xml"

// XML parsing using expat lib - handlers
typedef struct
{
	std::string curr_name;
	std::map<std::string, std::string> *pFeatures;
	int depth;
} ParserUserData;

static void XMLCALL
charDataHandler(void *userData, const char *s, int len) {
	ParserUserData* pData = (ParserUserData*)userData;
	std::string value(s, len);
	std::string curr_name = pData->curr_name;

	if (!curr_name.empty()) {
		std::map<std::string, std::string>::iterator result = (*pData->pFeatures).find(curr_name);

		if (result != (*pData->pFeatures).end()) {
			value = result->second + value;
			(*pData->pFeatures).erase(curr_name);
		}

		std::map<std::string, std::string>::iterator begin;
		begin = (*pData->pFeatures).begin();
		std::pair<std::string, std::string> feature(curr_name, value);
		(*pData->pFeatures).insert(begin, feature);
	}
}

static void XMLCALL
startElement(void *userData, const char *name, const char **atts __unused) {
	ParserUserData* pData = (ParserUserData*)userData;
	pData->curr_name.assign(name);
	pData->depth += 1;
}

static void XMLCALL
endElement(void *userData, const char *name __unused) {
	ParserUserData* pData = (ParserUserData*)userData;
	pData->curr_name.clear();
	pData->depth -= 1;
}

// SecNativeFeture class implementation
SecNativeFeature* SecNativeFeature::_instance = NULL;

SecNativeFeature::SecNativeFeature() {
	int load_result = -1;
	_features.clear();

	if (access(OMC_MPS_FEATURE_XML, R_OK) == 0) {
		load_result = _loadFeature(0, OMC_MPS_FEATURE_XML);
	}

	if (load_result == -1) {
		load_result = _loadFeature(FEATURE_XML, MPS_FEATURE_XML);
	}

	if (load_result == -1) {
		ALOGW("SecNativeFeature: Feature file not found");
	}
}

SecNativeFeature::~SecNativeFeature() {
	delete _instance;
}

SecNativeFeature* SecNativeFeature::getInstance() {
	if (_instance == NULL) {
		_instance = new SecNativeFeature();
	}
	return _instance;
}

bool SecNativeFeature::getEnableStatus(const char* tag) {
	std::map<std::string, std::string>::iterator found;
	found = _features.find(tag);

	if (found == _features.end()) {
		return false;
	}

	if (found->second.compare("true") == 0 || found->second.compare("TRUE") == 0) {
		return true;
	}
	return false;
}

bool SecNativeFeature::getEnableStatus(const char* tag, bool defaultValue) {
	std::map<std::string, std::string>::iterator found;
	found = _features.find(tag);

	if (found == _features.end()) {
		return defaultValue;
	}

	if (found->second.compare("true") == 0 || found->second.compare("TRUE") == 0) {
		return true;
	}
	return defaultValue;
}

const char* SecNativeFeature::getString(const char* tag) {
	std::map<std::string, std::string>::iterator found;
	found = _features.find(tag);

	if (found == _features.end()) {
		return "";
	}
	return found->second.c_str();
}

const char* SecNativeFeature::getString(const char* tag, char* defaultValue) {
	std::map<std::string, std::string>::iterator found;
	found = _features.find(tag);

	if (found == _features.end()) {
		return defaultValue;
	}
	return found->second.c_str();
}

int SecNativeFeature::getInteger(const char* tag) {
	std::map<std::string, std::string>::iterator found;
	found = _features.find(tag);

	if (found == _features.end()) {
		if (strcmp(tag, TAG_CSCFEATURE_RIL_CONFIGRILVERSION) == 0) {
			return 11;
		}
		return -1;
	}
	std::string raw_value = _features.find(tag)->second;
	return atoi(raw_value.c_str());
}

int SecNativeFeature::getInteger(const char* tag, int defaultValue) {
	std::map<std::string, std::string>::iterator found;
	found = _features.find(tag);

	if (found == _features.end()) {
		if (strcmp(tag, TAG_CSCFEATURE_RIL_CONFIGRILVERSION) == 0) {
			return 11;
		}
		return defaultValue;
	}
	std::string raw_value = _features.find(tag)->second;
	return atoi(raw_value.c_str());
}

int SecNativeFeature::_loadFeature(const char* xmlPath, const char* xmlPath2) {
	char buf[BUFSIZ];
	XML_Parser parser = XML_ParserCreate(NULL);
	int done;
	FILE * pFeatureFile = NULL;
	ParserUserData userData;
	userData.curr_name = std::string();
	userData.pFeatures = &_features;
	userData.depth = 0;

	if (!xmlPath || (pFeatureFile = fopen(xmlPath, "r")) == NULL) {
		if (!xmlPath2 || (pFeatureFile = fopen(xmlPath2, "r")) == NULL) {
			return -1;
		}
	}

	XML_SetUserData(parser, &userData);
	XML_SetElementHandler(parser, startElement, endElement);
	XML_SetCharacterDataHandler(parser, charDataHandler);
	do {
		size_t len = fread(buf, 1, sizeof(buf), pFeatureFile);
		if ((len != sizeof(buf)) && (ferror(pFeatureFile))) {
			fclose(pFeatureFile);
			return -1;
		}
		done = len < sizeof(buf);
		if (XML_Parse(parser, buf, len, done) == XML_STATUS_ERROR) {
			if (pFeatureFile) {
				fclose(pFeatureFile);
			}
			return -1;
		}
	} while (!done);

	XML_ParserFree(parser);
	fclose(pFeatureFile);
	return 0;
}

