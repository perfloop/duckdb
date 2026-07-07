#include "catch.hpp"
#include "test_helpers.hpp"
#include "duckdb/execution/executor.hpp"
#include "duckdb/common/types/data_chunk.hpp"

using namespace duckdb;

TEST_CASE("Test Executor Chunk Pool and Transparent Comparator", "[api]") {
	DuckDB db(nullptr);
	Connection con(db);
	auto &context = *con.context;

	// Create an Executor
	Executor executor(context);

	// Let's get the default buffer allocator
	auto &allocator = BufferAllocator::Get(context);

	// Define some logical types
	// 1. Decimals with different scales/precisions to verify that they are correctly segregated!
	LogicalType dec1 = LogicalType::DECIMAL(18, 2);
	LogicalType dec2 = LogicalType::DECIMAL(18, 3);
	LogicalType dec3 = LogicalType::DECIMAL(15, 2);

	// 2. Struct types with different child types
	child_list_t<LogicalType> children_a;
	children_a.push_back(make_pair("a", LogicalType::INTEGER));
	LogicalType struct_a = LogicalType::STRUCT(children_a);

	child_list_t<LogicalType> children_b;
	children_b.push_back(make_pair("a", LogicalType::VARCHAR));
	LogicalType struct_b = LogicalType::STRUCT(children_b);

	// 3. Simple types
	LogicalType int_type = LogicalType::INTEGER;
	LogicalType varchar_type = LogicalType::VARCHAR;

	// Test the comparator by fetching and returning chunks of different types
	vector<LogicalType> types_dec1 = {dec1};
	vector<LogicalType> types_dec2 = {dec2};
	vector<LogicalType> types_dec3 = {dec3};
	vector<LogicalType> types_struct_a = {struct_a};
	vector<LogicalType> types_struct_b = {struct_b};
	vector<LogicalType> types_simple = {int_type, varchar_type};

	// Fetch a chunk for types_dec1
	auto chunk_dec1 = executor.FetchChunk(allocator, types_dec1);
	REQUIRE(chunk_dec1->GetTypes() == types_dec1);

	// Fetch a chunk for types_dec2
	auto chunk_dec2 = executor.FetchChunk(allocator, types_dec2);
	REQUIRE(chunk_dec2->GetTypes() == types_dec2);

	// Return both to the pool
	executor.ReturnChunk(allocator, types_dec1, std::move(chunk_dec1));
	executor.ReturnChunk(allocator, types_dec2, std::move(chunk_dec2));

	// Fetch again. They should be retrieved from the pool correctly and should NOT be mixed up!
	auto chunk_dec1_refetched = executor.FetchChunk(allocator, types_dec1);
	auto chunk_dec2_refetched = executor.FetchChunk(allocator, types_dec2);

	REQUIRE(chunk_dec1_refetched->GetTypes() == types_dec1);
	REQUIRE(chunk_dec2_refetched->GetTypes() == types_dec2);

	// Return them back
	executor.ReturnChunk(allocator, types_dec1, std::move(chunk_dec1_refetched));
	executor.ReturnChunk(allocator, types_dec2, std::move(chunk_dec2_refetched));

	// Verify that struct types are also segregated correctly
	auto chunk_struct_a = executor.FetchChunk(allocator, types_struct_a);
	auto chunk_struct_b = executor.FetchChunk(allocator, types_struct_b);

	REQUIRE(chunk_struct_a->GetTypes() == types_struct_a);
	REQUIRE(chunk_struct_b->GetTypes() == types_struct_b);

	executor.ReturnChunk(allocator, types_struct_a, std::move(chunk_struct_a));
	executor.ReturnChunk(allocator, types_struct_b, std::move(chunk_struct_b));

	auto chunk_struct_a_refetched = executor.FetchChunk(allocator, types_struct_a);
	auto chunk_struct_b_refetched = executor.FetchChunk(allocator, types_struct_b);

	REQUIRE(chunk_struct_a_refetched->GetTypes() == types_struct_a);
	REQUIRE(chunk_struct_b_refetched->GetTypes() == types_struct_b);
}
