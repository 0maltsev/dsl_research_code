#include "boundfin/source/eval/eval.hpp"
#include "boundfin_test.hpp"

namespace {

using namespace boundfin::source::eval;

// main.pdf p.13, Sec. 5.1: Reach_sigma(V) is the least identifier set
// closed under identifiers occurring in V and recursively in reached
// objects' own elements; sigma restricted-to V keeps only that closure.

SealedObject sealed_with(std::vector<Value> elements) {
  SealedObject object;
  object.capacity = 8;
  object.length = static_cast<std::uint32_t>(elements.size());
  object.origin = Origin::Local;
  object.elements = std::move(elements);
  return object;
}

StoreObject sealed_store_object(std::vector<Value> elements) {
  return StoreObject{StoreObjectKind::Sealed, sealed_with(std::move(elements))};
}

void reachable_objects_of_empty_roots_is_empty() {
  const Store store;
  const auto reachable = reachable_objects(store, {});
  BOUNDFIN_CHECK(reachable.empty());
}

void reachable_objects_ignores_scalar_roots() {
  const Store store;
  const std::vector<Value> roots{make_i32(5), make_bool(true), make_i64(7), make_f64(0)};
  const auto reachable = reachable_objects(store, roots);
  BOUNDFIN_CHECK(reachable.empty());
}

void reachable_objects_includes_a_direct_array_root() {
  Store store;
  store.emplace(1, sealed_store_object({}));
  const auto reachable = reachable_objects(store, {make_array(1)});
  BOUNDFIN_CHECK_EQ(reachable.size(), static_cast<std::size_t>(1));
  BOUNDFIN_CHECK(reachable.count(1) == 1);
}

void reachable_objects_recurses_through_product_components() {
  Store store;
  store.emplace(1, sealed_store_object({}));
  const auto root = make_product({make_i32(0), make_array(1)});
  const auto reachable = reachable_objects(store, {root});
  BOUNDFIN_CHECK(reachable.count(1) == 1);
}

// Object 1 (sealed) itself contains an element referencing object 2 --
// confirms the closure recurses through a SEALED object's own elements,
// not just the immediate root value.
void reachable_objects_recurses_through_sealed_elements() {
  Store store;
  store.emplace(2, sealed_store_object({}));
  store.emplace(1, sealed_store_object({make_array(2)}));
  const auto reachable = reachable_objects(store, {make_array(1)});
  BOUNDFIN_CHECK_EQ(reachable.size(), static_cast<std::size_t>(2));
  BOUNDFIN_CHECK(reachable.count(1) == 1);
  BOUNDFIN_CHECK(reachable.count(2) == 1);
}

// Object 1 (prefix, not yet sealed) contains an initialized element
// referencing object 2 -- confirms the closure also recurses through a
// PREFIX object's own initialized_elements (cost-trace-semantics.md
// Sec. 1: "A prefix exists only inside literal/builder administration").
void reachable_objects_recurses_through_prefix_elements() {
  Store store;
  store.emplace(2, sealed_store_object({}));
  PrefixObject prefix;
  prefix.capacity = 8;
  prefix.target_length = 4;
  prefix.initialized_count = 1;
  prefix.origin = Origin::Local;
  prefix.initialized_elements = {make_array(2)};
  store.emplace(1, StoreObject{StoreObjectKind::Prefix, prefix});
  const auto reachable = reachable_objects(store, {make_array(1)});
  BOUNDFIN_CHECK_EQ(reachable.size(), static_cast<std::size_t>(2));
  BOUNDFIN_CHECK(reachable.count(2) == 1);
}

// main.pdf's own definition ("the least set containing every array
// identifier occurring in V...") includes a root's own identifier
// UNCONDITIONALLY, even one absent from the store -- only RECURSING
// further into that id's own elements requires it to actually be
// present. A spec-auditor review is specifically directed to confirm
// this reading (the first version of this test wrongly expected an
// empty result, fixed after re-reading the definition literally).
void reachable_objects_includes_a_dangling_root_unconditionally() {
  const Store store; // empty -- 999 names nothing
  const auto reachable = reachable_objects(store, {make_array(999)});
  BOUNDFIN_CHECK_EQ(reachable.size(), static_cast<std::size_t>(1));
  BOUNDFIN_CHECK(reachable.count(999) == 1);
}

// restrict_store's own output is naturally unaffected by a dangling root:
// there is nothing in `store` to actually copy for an id that was never
// present, so the restricted store simply does not contain it either.
void restrict_store_of_a_dangling_root_contains_nothing() {
  const Store store; // empty
  const auto restricted = restrict_store(store, {make_array(999)});
  BOUNDFIN_CHECK(restricted.empty());
}

// The key "semantic quotienting" behavior (main.pdf p.14): an object NOT
// in the reachable closure from the given roots is dropped entirely from
// the restricted store, even though it was present in the original store.
void restrict_store_drops_unreachable_objects() {
  Store store;
  store.emplace(1, sealed_store_object({})); // reachable
  store.emplace(2, sealed_store_object({})); // NOT reachable from roots below
  const auto restricted = restrict_store(store, {make_array(1)});
  BOUNDFIN_CHECK_EQ(restricted.size(), static_cast<std::size_t>(1));
  BOUNDFIN_CHECK(restricted.count(1) == 1);
  BOUNDFIN_CHECK(restricted.count(2) == 0);
}

void restrict_store_of_empty_roots_drops_everything() {
  Store store;
  store.emplace(1, sealed_store_object({}));
  const auto restricted = restrict_store(store, {});
  BOUNDFIN_CHECK(restricted.empty());
}

void boundfin_eval_store_reachability() {
  reachable_objects_of_empty_roots_is_empty();
  reachable_objects_ignores_scalar_roots();
  reachable_objects_includes_a_direct_array_root();
  reachable_objects_recurses_through_product_components();
  reachable_objects_recurses_through_sealed_elements();
  reachable_objects_recurses_through_prefix_elements();
  reachable_objects_includes_a_dangling_root_unconditionally();
  restrict_store_of_a_dangling_root_contains_nothing();
  restrict_store_drops_unreachable_objects();
  restrict_store_of_empty_roots_drops_everything();
}

} // namespace

BOUNDFIN_TEST_MAIN(boundfin_eval_store_reachability)
