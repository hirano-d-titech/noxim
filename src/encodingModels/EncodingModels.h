#ifndef __NOXIMENCODINGMODELS_H__
#define __NOXIMENCODINGMODELS_H__

#include <map>
#include <string>
#include "EncodingModel.h"

using namespace std;

typedef map<string, EncodingModel* > EncodingModelsMap;

class EncodingModels {
    public:
        static EncodingModelsMap * encodingModelsMap;
        static EncodingModelsMap * getEncodingModelsMap();

        static EncodingModel * get(const string & encodingModelName);
};

struct EncodingModelsRegister : EncodingModels {
    EncodingModelsRegister(const string & encodingModelName, EncodingModel * encodingModel) {
        getEncodingModelsMap()->insert(make_pair(encodingModelName, encodingModel));
    }
};

#endif
