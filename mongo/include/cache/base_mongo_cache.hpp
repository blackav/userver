#pragma once

/// @file cache/base_mongo_cache.hpp
/// @brief @copybrief components::MongoCache

#include <cache/cache_statistics.hpp>
#include <cache/caching_component_base.hpp>

// Autogenerated
#include <storages/mongo_collections/component.hpp>

namespace components {

// clang-format off

/// @brief %Base class for all caches polling mongo collection
///
/// Example of traits for MongoCache:
///
/// struct MongoCacheTraitsExample {
///   // Component name for component
///   static constexpr auto kName = "mongo-taxi-config";
///
///   // Collection to read from
///   static constexpr auto kMongoCollectionsField =
///       &storages::mongo::Collections::config;
///   // Update field name to use for incremental update
///   // Please use reference here to avoid global variables
///   // initialization order issues.
///   static constexpr const std::string& kMongoUpdateFieldName =
///       mongo::db::taxi::config::kUpdated;
///
///   // Cache element type
///   using ObjectType = MongoDocument;
///   // Cache element field name that is used as an index in the cache map
///   static constexpr auto kKeyField = &MongoDocument::name;
///   // Type of kKeyField
///   using KeyType = std::string;
///   // Type of cache map, e.g. unordered_map, map, bimap
///   using DataType = std::unordered_map<KeyType, ObjectType>;
///
///   // Function that converts BSON to ObjectType
///   static constexpr auto kDeserializeFunc = &MongoDocument::FromBson;
///   // Whether it is OK to read from replica (if true, you might get stale data)
///   static constexpr bool kIsSecondaryPreferred = true;
///
///   // Whether update part of the cache even if failed to parse some documents
///   static constexpr bool kAreInvalidDocumentsSkipped = false;
/// };

// clang-format on

template <class MongoCacheTraits>
class MongoCache
    : public CachingComponentBase<typename MongoCacheTraits::DataType> {
 public:
  static constexpr const char* kName = MongoCacheTraits::kName;

  MongoCache(const ComponentConfig&, const ComponentContext&);

  ~MongoCache();

 private:
  void Update(cache::UpdateType type,
              const std::chrono::system_clock::time_point& last_update,
              const std::chrono::system_clock::time_point& now,
              cache::UpdateStatisticsScope& stats_scope) override;

  storages::mongo::DocumentValue GetQuery(
      cache::UpdateType type,
      const std::chrono::system_clock::time_point& last_update);

  std::shared_ptr<typename MongoCacheTraits::DataType> GetData(
      cache::UpdateType type);

  storages::mongo::CollectionsPtr mongo_collections_;
  storages::mongo::Collection* mongo_collection_;
};

template <class MongoCacheTraits>
MongoCache<MongoCacheTraits>::MongoCache(const ComponentConfig& config,
                                         const ComponentContext& context)
    : CachingComponentBase<typename MongoCacheTraits::DataType>(
          config, context, MongoCacheTraits::kName) {
  auto& mongo_component = context.FindComponent<components::MongoCollections>();
  mongo_collections_ = mongo_component.GetCollections();
  mongo_collection_ =
      &((*mongo_collections_).*MongoCacheTraits::kMongoCollectionsField);

  // TODO: update CacheConfig from TaxiConfig

  this->StartPeriodicUpdates();
}

template <class MongoCacheTraits>
MongoCache<MongoCacheTraits>::~MongoCache() {
  this->StopPeriodicUpdates();
}

template <class MongoCacheTraits>
void MongoCache<MongoCacheTraits>::Update(
    cache::UpdateType type,
    const std::chrono::system_clock::time_point& last_update,
    const std::chrono::system_clock::time_point& /*now*/,
    cache::UpdateStatisticsScope& stats_scope) {
  namespace bbb = bsoncxx::builder::basic;
  namespace sm = storages::mongo;

  auto* collection = mongo_collection_;

  auto query = GetQuery(type, last_update);

  mongocxx::read_preference read_pref;
  if (MongoCacheTraits::kIsSecondaryPreferred)
    read_pref.mode(mongocxx::read_preference::read_mode::k_secondary_preferred);
  mongocxx::options::find options;
  options.read_preference(std::move(read_pref));

  auto cursor = collection->Find(query).Get();
  auto it = cursor.begin();
  if (type == cache::UpdateType::kIncremental && it == cursor.end()) {
    // Don't touch the cache at all
    LOG_INFO() << "No changes in cache " << MongoCacheTraits::kName;
    stats_scope.FinishNoChanges();
    return;
  }

  auto new_cache = GetData(type);
  for (; it != cursor.end(); ++it) {
    const auto& doc = *it;
    stats_scope.IncreaseDocumentsReadCount(1);

    try {
      auto object = MongoCacheTraits::kDeserializeFunc(sm::DocumentValue(doc));
      auto key = (object.*MongoCacheTraits::kKeyField);

      if (type == cache::UpdateType::kIncremental ||
          new_cache->count(key) == 0) {
        (*new_cache)[key] = std::move(object);
      } else {
        LOG_ERROR() << "Found duplicate key for 2 items in cache "
                    << MongoCacheTraits::kName << ", key=" << key;
      }
    } catch (const std::exception& e) {
      LOG_ERROR() << "Failed to deserialize cache item of cache "
                  << MongoCacheTraits::kName
                  << ", _id=" << sm::ToString(doc["_id"]) << ", what(): " << e;
      stats_scope.IncreaseDocumentsParseFailures(1);

      if (!MongoCacheTraits::kAreInvalidDocumentsSkipped) throw;
    }
  }

  this->Set(new_cache);
  stats_scope.Finish(new_cache->size());
}

template <class MongoCacheTraits>
storages::mongo::DocumentValue MongoCache<MongoCacheTraits>::GetQuery(
    cache::UpdateType type,
    const std::chrono::system_clock::time_point& last_update) {
  namespace bbb = bsoncxx::builder::basic;
  namespace sm = storages::mongo;

  if (type == cache::UpdateType::kFull) return sm::kEmptyObject;

  return bbb::make_document(
      bbb::kvp(std::string(MongoCacheTraits::kMongoUpdateFieldName),
               [last_update](bbb::sub_document subdoc) {
                 subdoc.append(bbb::kvp(std::string("$gt"),
                                        bsoncxx::types::b_date(last_update)));
               }));
}

template <class MongoCacheTraits>
std::shared_ptr<typename MongoCacheTraits::DataType>
MongoCache<MongoCacheTraits>::GetData(cache::UpdateType type) {
  if (type == cache::UpdateType::kIncremental)
    return std::make_shared<typename MongoCacheTraits::DataType>(*this->Get());
  else
    return std::make_shared<typename MongoCacheTraits::DataType>();
}

}  // namespace components
