#include "EncodingModels.h"

EncodingModelsMap * EncodingModels::encodingModelsMap = 0;

EncodingModel * EncodingModels::get(const string & encodingModelName) {
    EncodingModelsMap::iterator it = getEncodingModelsMap()->find(encodingModelName);

    if (it == getEncodingModelsMap()->end())
        return 0;

    return it->second;
}

EncodingModelsMap * EncodingModels::getEncodingModelsMap() {
    if (encodingModelsMap == 0)
        encodingModelsMap = new EncodingModelsMap();
    return encodingModelsMap;
}
