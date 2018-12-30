/* ---------------------------------------------------------------------
 * Copyright (C) 2018, David McDougall.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU Affero Public License for more details.
 *
 * You should have received a copy of the GNU Affero Public License
 * along with this program.  If not, see http://www.gnu.org/licenses.
 * ----------------------------------------------------------------------
 */

#include <gtest/gtest.h>
#include <nupic/ntypes/Sdr.hpp>
#include <vector>
#include <random>

using namespace std;
using namespace nupic;

/* This also tests the size and dimensions are correct */
TEST(SdrTest, TestConstructor) {
    // Test 0 size
    ASSERT_ANY_THROW( SDR( vector<UInt>(0) ));
    ASSERT_ANY_THROW( SDR({ 0 }) );
    ASSERT_ANY_THROW( SDR({ 3, 2, 1, 0 }) );

    // Test 1-D
    vector<UInt> b_dims = {3};
    SDR b(b_dims);
    ASSERT_EQ( b.size, 3ul );
    ASSERT_EQ( b.dimensions, b_dims );
    ASSERT_EQ( b.getSparse().size(), 1ul );
    // zero initialized
    ASSERT_EQ( b.getDense(),     vector<Byte>({0, 0, 0}) );
    ASSERT_EQ( b.getFlatSparse(), vector<UInt>(0) );
    ASSERT_EQ( b.getSparse(),     vector<vector<UInt>>({{}}) );

    // Test 3-D
    vector<UInt> c_dims = {11u, 15u, 3u};
    SDR c(c_dims);
    ASSERT_EQ( c.size, 11u * 15u * 3u );
    ASSERT_EQ( c.dimensions, c_dims );
    ASSERT_EQ( c.getSparse().size(), 3ul );
    ASSERT_EQ( c.getFlatSparse().size(), 0ul );
    // Test dimensions are copied not referenced
    c_dims.push_back(7);
    ASSERT_EQ( c.dimensions, vector<UInt>({11u, 15u, 3u}) );
}

TEST(SdrTest, TestConstructorCopy) {
    // Test value/no-value is preserved
    SDR x({23});
    SDR x_copy(x);
    ASSERT_TRUE( x == x_copy );
    x.zero();
    SDR x_copy2 = SDR(x);
    ASSERT_TRUE( x == x_copy2 );

    // Test data is copied.
    SDR a({5});
    a.setDense( SDR_dense_t({0, 1, 0, 0, 0}));
    SDR b(a);
    ASSERT_EQ( b.getFlatSparse(),  vector<UInt>({1}) );
    ASSERT_TRUE(a == b);
}

TEST(SdrTest, TestZero) {
    SDR a({4, 4});
    a.setDense( vector<Byte>(16, 1) );
    a.zero();
    ASSERT_EQ( vector<Byte>(16, 0), a.getDense() );
    ASSERT_EQ( a.getFlatSparse().size(),  0ul);
    ASSERT_EQ( a.getSparse().size(),  2ul);
    ASSERT_EQ( a.getSparse().at(0).size(),  0ul);
    ASSERT_EQ( a.getSparse().at(1).size(),  0ul);
}

TEST(SdrTest, TestSDR_Examples) {
    // Make an SDR with 9 values, arranged in a (3 x 3) grid.
    // "SDR" is an alias/typedef for SparseDistributedRepresentation.
    SDR  X( {3, 3} );
    vector<Byte> data({
        0, 1, 0,
        0, 1, 0,
        0, 0, 1 });

    // These three statements are equivalent.
    X.setDense(SDR_dense_t({ 0, 1, 0,
                             0, 1, 0,
                             0, 0, 1 }));
    ASSERT_EQ( data, X.getDense() );
    X.setFlatSparse(SDR_flatSparse_t({ 1, 4, 8 }));
    ASSERT_EQ( data, X.getDense() );
    X.setSparse(SDR_sparse_t({{ 0, 1, 2,}, { 1, 1, 2 }}));
    ASSERT_EQ( data, X.getDense() );

    // Access data in any format, SDR will automatically convert data formats.
    ASSERT_EQ( X.getDense(),      SDR_dense_t({ 0, 1, 0, 0, 1, 0, 0, 0, 1 }) );
    ASSERT_EQ( X.getSparse(),     SDR_sparse_t({{ 0, 1, 2 }, {1, 1, 2}}) );
    ASSERT_EQ( X.getFlatSparse(), SDR_flatSparse_t({ 1, 4, 8 }) );

    // Data format conversions are cached, and when an SDR value changes the
    // cache is cleared.
    X.setFlatSparse(SDR_flatSparse_t({}));  // Assign new data to the SDR, clearing the cache.
    X.getDense();        // This line will convert formats.
    X.getDense();        // This line will resuse the result of the previous line

    X.zero();
    Byte *before = X.getDense().data();
    SDR_dense_t newData({ 1, 0, 0, 1, 0, 0, 1, 0, 0 });
    Byte *data_ptr = newData.data();
    X.setDense( newData );
    Byte *after = X.getDense().data();
    // X now points to newData, and newData points to X's old data.
    ASSERT_EQ( after, data_ptr );
    ASSERT_EQ( newData.data(), before );
    ASSERT_NE( before, after );

    X.zero();
    before = X.getDense().data();
    // The "&" is really important!  Otherwise vector copies.
    auto & dense = X.getDense();
    dense[2] = true;
    X.setDense( dense );              // Notify the SDR of the changes.
    after = X.getDense().data();
    ASSERT_EQ( X.getFlatSparse(), SDR_flatSparse_t({ 2 }) );
    ASSERT_EQ( before, after );
}

TEST(SdrTest, TestSetDenseVec) {
    SDR a({11, 10, 4});
    Byte *before = a.getDense().data();
    SDR_dense_t vec = vector<Byte>(440, 1);
    Byte *data = vec.data();
    a.setDense( vec );
    Byte *after = a.getDense().data();
    ASSERT_NE( before, after ); // not a copy.
    ASSERT_EQ( after, data ); // correct data buffer.
}

TEST(SdrTest, TestSetDenseByte) {
    SDR a({11, 10, 4});
    auto vec = vector<Byte>(a.size, 1);
    a.zero();
    a.setDense( (Byte*) vec.data());
    ASSERT_EQ( a.getDense(), vec );
    ASSERT_NE( ((vector<Byte>&) a.getDense()).data(), vec.data() ); // true copy not a reference
    ASSERT_EQ( a.getDense().data(), a.getDense().data() ); // But not a copy every time.
}

TEST(SdrTest, TestSetDenseUInt) {
    SDR a({11, 10, 4});
    auto vec = vector<UInt>(a.size, 1);
    a.setDense( (UInt*) vec.data() );
    ASSERT_EQ( a.getDense(), vector<Byte>(a.size, 1) );
    ASSERT_NE( a.getDense().data(), (const char*) vec.data()); // true copy not a reference
}

TEST(SdrTest, TestSetDenseArray) {
    // Test Byte sized data
    SDR A({ 3, 3 });
    vector<Byte> vec_byte({ 0, 1, 0, 0, 1, 0, 0, 0, 1 });
    auto arr = Array(NTA_BasicType_Byte, vec_byte.data(), vec_byte.size());
    A.setDense( arr );
    ASSERT_EQ( A.getFlatSparse(), SDR_flatSparse_t({ 1, 4, 8 }));

    // Test UInt64 sized data
    A.zero();
    vector<UInt64> vec_uint({ 1, 1, 0, 0, 1, 0, 0, 0, 1 });
    auto arr_uint64 = Array(NTA_BasicType_UInt64, vec_uint.data(), vec_uint.size());
    A.setDense( arr_uint64 );
    ASSERT_EQ( A.getFlatSparse(), SDR_flatSparse_t({ 0, 1, 4, 8 }));

    // Test Real sized data
    A.zero();
    vector<Real> vec_real({ 1., 1., 0., 0., 1., 0., 0., 0., 1. });
    auto arr_real = Array(NTA_BasicType_Real, vec_real.data(), vec_real.size());
    A.setDense( arr_real );
    ASSERT_EQ( A.getFlatSparse(), SDR_flatSparse_t({ 0, 1, 4, 8 }));
}

TEST(SdrTest, TestSetDenseInplace) {
    SDR a({10, 10});
    auto& a_data = a.getDense();
    ASSERT_EQ( a_data, vector<Byte>(100, 0) );
    a_data.assign( a.size, 1 );
    a.setDense( a_data );
    ASSERT_EQ( a.getDense().data(), a.getDense().data() );
    ASSERT_EQ( a.getDense().data(), a_data.data() );
    ASSERT_EQ( a.getDense(), vector<Byte>(a.size, 1) );
    ASSERT_EQ( a.getDense(), a_data );
}

TEST(SdrTest, TestSetFlatSparseVec) {
    SDR a({11, 10, 4});
    UInt *before = a.getFlatSparse().data();
    auto vec = vector<UInt>(a.size, 1);
    UInt *data = vec.data();
    for(UInt i = 0; i < a.size; i++)
        vec[i] = i;
    a.setFlatSparse( vec );
    UInt *after = a.getFlatSparse().data();
    ASSERT_NE( before, after );
    ASSERT_EQ( after, data );
}

TEST(SdrTest, TestSetFlatSparsePtr) {
    SDR a({11, 10, 4});
    auto vec = vector<UInt>(a.size, 1);
    for(UInt i = 0; i < a.size; i++)
        vec[i] = i;
    a.zero();
    a.setFlatSparse( (UInt*) vec.data(), a.size );
    ASSERT_EQ( a.getFlatSparse(), vec );
    ASSERT_NE( a.getFlatSparse().data(), vec.data()); // true copy not a reference
}

TEST(SdrTest, TestSetFlatSparseArray) {
    SDR A({ 3, 3 });
    // Test UInt32 sized data
    vector<UInt32> vec_uint32({ 1, 4, 8 });
    auto arr_uint32 = Array(NTA_BasicType_UInt32, vec_uint32.data(), vec_uint32.size());
    A.setFlatSparse( arr_uint32 );
    ASSERT_EQ( A.getDense(), SDR_dense_t({ 0, 1, 0, 0, 1, 0, 0, 0, 1 }));

    // Test UInt64 sized data
    A.zero();
    vector<UInt64> vec_uint64({ 1, 4, 8 });
    auto arr_uint64 = Array(NTA_BasicType_UInt64, vec_uint64.data(), vec_uint64.size());
    A.setFlatSparse( arr_uint64 );
    ASSERT_EQ( A.getDense(), SDR_dense_t({ 0, 1, 0, 0, 1, 0, 0, 0, 1 }));

    // Test Real sized data
    A.zero();
    vector<Real> vec_real({ 1, 4, 8 });
    auto arr_real = Array(NTA_BasicType_Real, vec_real.data(), vec_real.size());
    A.setFlatSparse( arr_real );
    ASSERT_EQ( A.getDense(), SDR_dense_t({ 0, 1, 0, 0, 1, 0, 0, 0, 1 }));
}

TEST(SdrTest, TestSetFlatSparseInplace) {
    // Test both mutable & inplace methods at the same time, which is the intended use case.
    SDR a({10, 10});
    a.zero();
    auto& a_data = a.getFlatSparse();
    ASSERT_EQ( a_data, vector<UInt>(0) );
    a_data.push_back(0);
    a.setFlatSparse( a_data );
    ASSERT_EQ( a.getFlatSparse().data(), a.getFlatSparse().data() );
    ASSERT_EQ( a.getFlatSparse(),        a.getFlatSparse() );
    ASSERT_EQ( a.getFlatSparse().data(), a_data.data() );
    ASSERT_EQ( a.getFlatSparse(),        a_data );
    ASSERT_EQ( a.getFlatSparse(), vector<UInt>(1) );
    a_data.clear();
    a.setFlatSparse( a_data );
    ASSERT_EQ( a.getDense(), vector<Byte>(a.size, 0) );
}

TEST(SdrTest, TestSetSparse) {
    SDR a({4, 1, 3});
    void *before = a.getSparse().data();
    auto vec = vector<vector<UInt>>({
        { 0, 1, 2, 0 },
        { 0, 0, 0, 0 },
        { 0, 1, 2, 1 } });
    void *data = vec.data();
    a.setSparse( vec );
    void *after = a.getSparse().data();
    ASSERT_EQ( after, data );
    ASSERT_NE( before, after );
}

TEST(SdrTest, TestSetSparseCopy) {
    SDR a({ 3, 3 });
    void *before = a.getSparse().data();
    auto vec = vector<vector<Real>>({
        { 0., 1., 2. },
        { 1., 1., 2. } });
    void *data = vec.data();
    a.setSparse( vec );
    void *after = a.getSparse().data();
    ASSERT_EQ( before, after );  // Data copied from vec into sdr's buffer
    ASSERT_NE( after, data );   // Data copied from vec into sdr's buffer
    ASSERT_EQ( a.getFlatSparse(), SDR_flatSparse_t({ 1, 4, 8 }));
}

TEST(SdrTest, TestSetSparseInplace) {
    // Test both mutable & inplace methods at the same time, which is the intended use case.
    SDR a({10, 10});
    a.zero();
    auto& a_data = a.getSparse();
    ASSERT_EQ( a_data, vector<vector<UInt>>({{}, {}}) );
    a_data[0].push_back(0);
    a_data[1].push_back(0);
    a_data[0].push_back(3);
    a_data[1].push_back(7);
    a_data[0].push_back(7);
    a_data[1].push_back(1);
    a.setSparse( a_data );
    ASSERT_EQ( a.getSum(), 3ul );
    // I think some of these check the same things but thats ok.
    ASSERT_EQ( (void*) a.getSparse().data(), (void*) a.getSparse().data() );
    ASSERT_EQ( a.getSparse(), a.getSparse() );
    ASSERT_EQ( a.getSparse().data(), a_data.data() );
    ASSERT_EQ( a.getSparse(),        a_data );
    ASSERT_EQ( a.getFlatSparse(), vector<UInt>({0, 37, 71}) ); // Check data ok
    a_data[0].clear();
    a_data[1].clear();
    a.setSparse( a_data );
    ASSERT_EQ( a.getDense(), vector<Byte>(a.size, 0) );
}

TEST(SdrTest, TestSetSDR) {
    SDR a({5});
    SDR b({5});
    // Test dense assignment works
    a.setDense(SDR_dense_t({1, 1, 1, 1, 1}));
    b.setSDR(a);
    ASSERT_EQ( b.getFlatSparse(), vector<UInt>({0, 1, 2, 3, 4}) );
    // Test flat sparse assignment works
    a.setFlatSparse(SDR_flatSparse_t({0, 1, 2, 3, 4}));
    b.setSDR(a);
    ASSERT_EQ( b.getDense(), vector<Byte>({1, 1, 1, 1, 1}) );
    // Test sparse assignment works
    a.setSparse(SDR_sparse_t({{0, 1, 2, 3, 4}}));
    b.setSDR(a);
    ASSERT_EQ( b.getDense(), vector<Byte>({1, 1, 1, 1, 1}) );
}

TEST(SdrTest, TestGetDenseFromFlatSparse) {
    // Test zeros
    SDR z({4, 4});
    z.setFlatSparse(SDR_flatSparse_t({}));
    ASSERT_EQ( z.getDense(), vector<Byte>(16, 0) );

    // Test ones
    SDR nz({4, 4});
    nz.setFlatSparse(SDR_flatSparse_t(
        {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15}));
    ASSERT_EQ( nz.getDense(), vector<Byte>(16, 1) );

    // Test 1-D
    SDR d1({30});
    d1.setFlatSparse(SDR_flatSparse_t({1, 29, 4, 5, 7}));
    vector<Byte> ans(30, 0);
    ans[1] = 1;
    ans[29] = 1;
    ans[4] = 1;
    ans[5] = 1;
    ans[7] = 1;
    ASSERT_EQ( d1.getDense(), ans );

    // Test 3-D
    SDR d3({10, 10, 10});
    d3.setFlatSparse(SDR_flatSparse_t({0, 5, 50, 55, 500, 550, 555, 999}));
    vector<Byte> ans2(1000, 0);
    ans2[0]   = 1;
    ans2[5]   = 1;
    ans2[50]  = 1;
    ans2[55]  = 1;
    ans2[500] = 1;
    ans2[550] = 1;
    ans2[555] = 1;
    ans2[999] = 1;
    ASSERT_EQ( d3.getDense(), ans2 );
}

TEST(SdrTest, TestGetDenseFromSparse) {
    // Test simple 2-D
    SDR a({3, 3});
    a.setSparse(SDR_sparse_t({{1, 0, 2}, {2, 0, 2}}));
    vector<Byte> ans(9, 0);
    ans[0] = 1;
    ans[5] = 1;
    ans[8] = 1;
    ASSERT_EQ( a.getDense(), ans );

    // Test zeros
    SDR z({99, 1});
    z.setSparse(SDR_sparse_t({{}, {}}));
    ASSERT_EQ( z.getDense(), vector<Byte>(99, 0) );
}

TEST(SdrTest, TestGetFlatSparseFromDense) {
    // Test simple 2-D SDR.
    SDR a({3, 3}); a.zero();
    auto dense = a.getDense();
    dense[5] = 1;
    dense[8] = 1;
    a.setDense(dense);
    ASSERT_EQ(a.getFlatSparse().at(0), 5ul);
    ASSERT_EQ(a.getFlatSparse().at(1), 8ul);

    // Test zero'd SDR.
    a.setDense( vector<Byte>(a.size, 0) );
    ASSERT_EQ( a.getFlatSparse().size(), 0ul );
}

TEST(SdrTest, TestGetFlatSparseFromSparse) {
    // Test simple 2-D SDR.
    SDR a({3, 3}); a.zero();
    auto& index = a.getSparse();
    ASSERT_EQ( index.size(), 2ul );
    ASSERT_EQ( index[0].size(), 0ul );
    ASSERT_EQ( index[1].size(), 0ul );
    // Insert flat index 4
    index.at(0).push_back(1);
    index.at(1).push_back(1);
    // Insert flat index 8
    index.at(0).push_back(2);
    index.at(1).push_back(2);
    // Insert flat index 5
    index.at(0).push_back(1);
    index.at(1).push_back(2);
    a.setSparse( index );
    ASSERT_EQ(a.getFlatSparse().at(0), 4ul);
    ASSERT_EQ(a.getFlatSparse().at(1), 8ul);
    ASSERT_EQ(a.getFlatSparse().at(2), 5ul);

    // Test zero'd SDR.
    a.setSparse(SDR_sparse_t( {{}, {}} ));
    ASSERT_EQ( a.getFlatSparse().size(), 0ul );
}

TEST(SdrTest, TestGetSparseFromFlat) {
    // Test simple 2-D SDR.
    SDR a({3, 3}); a.zero();
    auto& index = a.getSparse();
    ASSERT_EQ( index.size(), 2ul );
    ASSERT_EQ( index[0].size(), 0ul );
    ASSERT_EQ( index[1].size(), 0ul );
    a.setFlatSparse(SDR_flatSparse_t({ 4, 8, 5 }));
    ASSERT_EQ( a.getSparse(), vector<vector<UInt>>({
        { 1, 2, 1 },
        { 1, 2, 2 } }) );

    // Test zero'd SDR.
    a.setFlatSparse(SDR_flatSparse_t( { } ));
    ASSERT_EQ( a.getSparse(), vector<vector<UInt>>({{}, {}}) );
}

TEST(SdrTest, TestGetSparseFromDense) {
    // Test simple 2-D SDR.
    SDR a({3, 3}); a.zero();
    auto dense = a.getDense();
    dense[5] = 1;
    dense[8] = 1;
    a.setDense(dense);
    ASSERT_EQ( a.getSparse(), vector<vector<UInt>>({
        { 1, 2 },
        { 2, 2 }}) );

    // Test zero'd SDR.
    a.setDense( vector<Byte>(a.size, 0) );
    ASSERT_EQ( a.getSparse()[0].size(), 0ul );
    ASSERT_EQ( a.getSparse()[1].size(), 0ul );
}

TEST(SdrTest, TestAt) {
    SDR a({3, 3});
    a.setFlatSparse(SDR_flatSparse_t( {4, 5, 8} ));
    ASSERT_TRUE( a.at( {1, 1} ));
    ASSERT_TRUE( a.at( {1, 2} ));
    ASSERT_TRUE( a.at( {2, 2} ));
    ASSERT_FALSE( a.at( {0 , 0} ));
    ASSERT_FALSE( a.at( {0 , 1} ));
    ASSERT_FALSE( a.at( {0 , 2} ));
    ASSERT_FALSE( a.at( {1 , 0} ));
    ASSERT_FALSE( a.at( {2 , 0} ));
    ASSERT_FALSE( a.at( {2 , 1} ));
}

TEST(SdrTest, TestSumSparsity) {
    SDR a({31, 17, 3});
    auto& dense = a.getDense();
    for(UInt i = 0; i < a.size; i++) {
        ASSERT_EQ( i, a.getSum() );
        EXPECT_FLOAT_EQ( (Real) i / a.size, a.getSparsity() );
        dense[i] = 1;
        a.setDense( dense );
    }
    ASSERT_EQ( a.size, a.getSum() );
    ASSERT_FLOAT_EQ( 1, a.getSparsity() );
}

TEST(SdrTest, TestPrint) {
    stringstream str;
    SDR a({100});
    a.print(str);
    // Use find so that trailing whitespace differences on windows/unix don't break it.
    ASSERT_NE( str.str().find( "SDR( 100 )" ), std::string::npos);

    stringstream str2;
    SDR b({ 9, 8 });
    b.print(str2);
    ASSERT_NE( str2.str().find( "SDR( 9, 8 )" ), std::string::npos);

    stringstream str3;
    SDR sdr3({ 3, 3 });
    sdr3.setDense(SDR_dense_t({ 0, 1, 0, 0, 1, 0, 0, 0, 1 }));
    sdr3.print(str3);
    ASSERT_NE( str3.str().find( "SDR( 3, 3 ) 1, 4, 8" ), std::string::npos);

    // Check that default aruments don't crash.
    cout << "PRINTING \"SDR( 3, 3 ) 1, 4, 8\" TO STDOUT: ";
    sdr3.print();
}

TEST(SdrTest, TestOverlap) {
    SDR a({3, 3});
    a.setDense(SDR_dense_t({1, 1, 1, 1, 1, 1, 1, 1, 1}));
    SDR b(a);
    ASSERT_EQ( a.overlap( b ), 9ul );
    b.zero();
    ASSERT_EQ( a.overlap( b ), 0ul );
    b.setDense(SDR_dense_t({0, 1, 0, 0, 1, 0, 0, 0, 1}));
    ASSERT_EQ( a.overlap( b ), 3ul );
    a.zero(); b.zero();
    ASSERT_EQ( a.overlap( b ), 0ul );
}

TEST(SdrTest, TestRandomize) {
    // Test sparsity is OK
    SDR a({1000});
    a.randomize( 0. );
    ASSERT_EQ( a.getSum(), 0ul );
    a.randomize( .25 );
    ASSERT_EQ( a.getSum(), 250ul );
    a.randomize( .5 );
    ASSERT_EQ( a.getSum(), 500ul );
    a.randomize( .75 );
    ASSERT_EQ( a.getSum(), 750ul );
    a.randomize( 1. );
    ASSERT_EQ( a.getSum(), 1000ul );
    // Test RNG is deterministic
    SDR b(a);
    Random rng(77);
    Random rng2(77);
    a.randomize( .02, rng );
    b.randomize( .02, rng2 );
    ASSERT_TRUE( a == b);
    // Test different random number generators have different results.
    Random rng3( 1 );
    Random rng4( 2 );
    a.randomize( .02, rng3 );
    b.randomize( .02, rng4 );
    ASSERT_TRUE( a != b);
    // Test that this modifies RNG state and will generate different
    // distributions with the same RNG.
    Random rng5( 88 );
    a.randomize( .02, rng5 );
    b.randomize( .02, rng5 );
    ASSERT_TRUE( a != b);
    // Test default RNG has a different result every time
    a.randomize( .02 );
    b.randomize( .02 );
    ASSERT_TRUE( a != b);
    // Methodically test by running it many times and checking for an even
    // activation frequency at every bit.
    SDR af_test({ 97 /* prime number */ });
    UInt iterations = 50000;
    Real sparsity   = .20;
    vector<Real> af( af_test.size, 0 );
    for( UInt i = 0; i < iterations; i++ ) {
        af_test.randomize( sparsity );
        for( auto idx : af_test.getFlatSparse() )
            af[ idx ] += 1;
    }
    for( auto f : af ) {
        f = f / iterations / sparsity;
        ASSERT_GT( f, 0.95 );
        ASSERT_LT( f, 1.05 );
    }
}

TEST(SdrTest, TestAddNoise) {
    SDR a({1000});
    a.randomize( 0.10 );
    SDR b(a);
    SDR c(a);
    // Test seed is deteministic
    b.setSDR(a);
    c.setSDR(a);
    Random b_rng( 44 );
    Random c_rng( 44 );
    b.addNoise( 0.5, b_rng );
    c.addNoise( 0.5, c_rng );
    ASSERT_TRUE( b == c );
    ASSERT_FALSE( a == b );
    // Test different seed generates different distributions
    b.setSDR(a);
    c.setSDR(a);
    Random rng1( 1 );
    Random rng2( 2 );
    b.addNoise( 0.5, rng1 );
    c.addNoise( 0.5, rng2 );
    ASSERT_TRUE( b != c );
    // Test addNoise changes PRNG state so two consequtive calls yeild different
    // results.
    Random prng( 55 );
    b.setSDR(a);
    b.addNoise( 0.5, prng );
    SDR b_cpy(b);
    b.setSDR(a);
    b.addNoise( 0.5, prng );
    ASSERT_TRUE( b_cpy != b );
    // Test default seed works ok
    b.setSDR(a);
    c.setSDR(a);
    b.addNoise( 0.5 );
    c.addNoise( 0.5 );
    ASSERT_TRUE( b != c );
    // Methodically test for every overlap.
    for( UInt x = 0; x <= 100; x++ ) {
        b.setSDR( a );
        b.addNoise( (Real)x / 100.0 );
        ASSERT_EQ( a.overlap( b ), 100 - x );
        ASSERT_EQ( b.getSum(), 100ul );
    }
}

TEST(SdrTest, TestEquality) {
    vector<SDR*> test_cases;
    // Test different dimensions
    test_cases.push_back( new SDR({ 11 }));
    test_cases.push_back( new SDR({ 1, 1 }));
    test_cases.push_back( new SDR({ 1, 2, 3 }));
    // Test different data
    test_cases.push_back( new SDR({ 3, 3 }));
    test_cases.back()->setDense(SDR_dense_t({0, 0, 1, 0, 1, 0, 1, 0, 0,}));
    test_cases.push_back( new SDR({ 3, 3 }));
    test_cases.back()->setDense(SDR_dense_t({0, 1, 0, 0, 1, 0, 0, 1, 0}));
    test_cases.push_back( new SDR({ 3, 3 }));
    test_cases.back()->setDense(SDR_dense_t({0, 1, 0, 0, 1, 0, 0, 0, 1}));
    test_cases.push_back( new SDR({ 3, 3 }));
    test_cases.back()->setFlatSparse(SDR_flatSparse_t({0,}));
    test_cases.push_back( new SDR({ 3, 3 }));
    test_cases.back()->setFlatSparse(SDR_flatSparse_t({3, 4, 6}));

    // Check that SDRs equal themselves
    for(UInt x = 0; x < test_cases.size(); x++) {
        for(UInt y = 0; y < test_cases.size(); y++) {
            SDR *a = test_cases[x];
            SDR *b = test_cases[y];
            if( x == y ) {
                ASSERT_TRUE(  *a == *b );
                ASSERT_FALSE( *a != *b );
            }
            else {
                ASSERT_TRUE(  *a != *b );
                ASSERT_FALSE( *a == *b );
            }
        }
    }

    for( SDR* z : test_cases )
        delete z;
}

TEST(SdrTest, TestSaveLoad) {
    const char *filename = "SdrSerialization.tmp";
    ofstream outfile;
    outfile.open(filename);

    // Test zero value
    SDR zero({ 3, 3 });
    zero.save( outfile );

    // Test dense data
    SDR dense({ 3, 3 });
    dense.setDense(SDR_dense_t({ 0, 1, 0, 0, 1, 0, 0, 0, 1 }));
    dense.save( outfile );

    // Test flat data
    SDR flat({ 3, 3 });
    flat.setFlatSparse(SDR_flatSparse_t({ 1, 4, 8 }));
    flat.save( outfile );

    // Test index data
    SDR index({ 3, 3 });
    index.setSparse(SDR_sparse_t({
            { 0, 1, 2 },
            { 1, 1, 2 }}));
    index.save( outfile );

    // Now load all of the data back into SDRs.
    outfile.close();
    ifstream infile( filename );

    if( false ) {
        // Print the file's contents
        std::stringstream buffer; buffer << infile.rdbuf();
        cout << buffer.str() << "EOF" << endl;
        infile.seekg( 0 ); // rewind to start of file.
    }

    SDR zero_2;
    zero_2.load( infile );
    SDR dense_2;
    dense_2.load( infile );
    SDR flat_2;
    flat_2.load( infile );
    SDR index_2;
    index_2.load( infile );

    infile.close();
    int ret = ::remove( filename );
    ASSERT_TRUE(ret == 0) << "Failed to delete " << filename;

    // Check that all of the data is OK
    ASSERT_TRUE( zero    == zero_2 );
    ASSERT_TRUE( dense   == dense_2 );
    ASSERT_TRUE( flat    == flat_2 );
    ASSERT_TRUE( index   == index_2 );
}

TEST(SdrTest, TestCallbacks) {

    SDR A({ 10, 20 });
    // Add and remove these callbacks a bunch of times, and then check they're
    // called the correct number of times.
    int count1 = 0;
    SDR_callback_t call1 = [&](){ count1++; };
    int count2 = 0;
    SDR_callback_t call2 = [&](){ count2++; };
    int count3 = 0;
    SDR_callback_t call3 = [&](){ count3++; };
    int count4 = 0;
    SDR_callback_t call4 = [&](){ count4++; };

    A.zero();   // No effect on callbacks
    A.zero();   // No effect on callbacks
    A.zero();   // No effect on callbacks

    UInt handle1 = A.addCallback( call1 );
    UInt handle2 = A.addCallback( call2 );
    UInt handle3 = A.addCallback( call3 );
    // Test proxies get callbacks
    SDR_Proxy C(A);
    C.addCallback( call4 );

    // Remove call 2 and add it back in.
    A.removeCallback( handle2 );
    A.zero();
    handle2 = A.addCallback( call2 );

    A.zero();
    ASSERT_EQ( count1, 2 );
    ASSERT_EQ( count2, 1 );
    ASSERT_EQ( count3, 2 );

    // Remove call 1
    A.removeCallback( handle1 );
    A.zero();
    ASSERT_EQ( count1, 2 );
    ASSERT_EQ( count2, 2 );
    ASSERT_EQ( count3, 3 );

    UInt handle2_2 = A.addCallback( call2 );
    UInt handle2_3 = A.addCallback( call2 );
    UInt handle2_4 = A.addCallback( call2 );
    UInt handle2_5 = A.addCallback( call2 );
    UInt handle2_6 = A.addCallback( call2 );
    UInt handle2_7 = A.addCallback( call2 );
    UInt handle2_8 = A.addCallback( call2 );
    A.zero();
    ASSERT_EQ( count1, 2 );
    ASSERT_EQ( count2, 10 );
    ASSERT_EQ( count3, 4 );

    A.removeCallback( handle2_2 );
    A.removeCallback( handle2_3 );
    A.removeCallback( handle2_4 );
    A.removeCallback( handle2_7 );
    A.removeCallback( handle2_6 );
    A.removeCallback( handle2_5 );
    A.removeCallback( handle2_8 );
    A.removeCallback( handle3 );
    A.removeCallback( handle2 );

    // Test removing junk handles.
    ASSERT_ANY_THROW( A.removeCallback( 99 ) );
    // Test callbacks are not copied.
    handle1 = A.addCallback( call1 );
    SDR B(A);
    ASSERT_ANY_THROW( B.removeCallback( handle1 ) );
    ASSERT_ANY_THROW( B.removeCallback( 0 ) );
    // Check proxy got all of the callbacks.
    ASSERT_EQ( count4, 4 );
}


TEST(SdrTest, TestProxyExamples) {
    SDR       A(    { 4, 4 });
    SDR_Proxy B( A, { 8, 2 });
    A.setSparse(SDR_sparse_t({{1, 1, 2}, {0, 1, 2}}));
    auto sparse = B.getSparse();
    ASSERT_EQ(sparse, SDR_sparse_t({{2, 2, 5}, {0, 1, 0}}));
}

TEST(SdrTest, TestProxyConstructor) {
    SDR         A({ 11 });
    SDR_Proxy   B( A );
    ASSERT_EQ( A.dimensions, B.dimensions );
    SDR_Proxy   C( A, { 11 });
    SDR         D({ 5, 4, 3, 2, 1 });
    SDR_Proxy   E( D, {1, 1, 1, 120, 1});
    SDR_Proxy   F( D, { 20, 6 });

    // Test that proxies can be safely made and destroyed.
    SDR_Proxy *G = new SDR_Proxy( A );
    SDR_Proxy *H = new SDR_Proxy( A );
    SDR_Proxy *I = new SDR_Proxy( A );
    A.zero();
    H->getDense();
    delete H;
    I->getDense();
    A.zero();
    SDR_Proxy *J = new SDR_Proxy( A );
    J->getDense();
    SDR_Proxy *K = new SDR_Proxy( A );
    delete K;
    SDR_Proxy *L = new SDR_Proxy( A );
    L->getSparse();
    delete L;
    delete G;
    I->getSparse();
    delete I;
    delete J;
    A.getDense();

    // Test invalid dimensions
    ASSERT_ANY_THROW( new SDR_Proxy( A, {2, 5}) );
    ASSERT_ANY_THROW( new SDR_Proxy( A, {11, 0}) );
}

TEST(SdrTest, TestProxyDeconstructor) {
    SDR       *A = new SDR({12});
    SDR_Proxy *B = new SDR_Proxy( *A );
    SDR_Proxy *C = new SDR_Proxy( *A, {3, 4} );
    SDR_Proxy *D = new SDR_Proxy( *C, {4, 3} );
    SDR_Proxy *E = new SDR_Proxy( *C, {2, 6} );
    D->getDense();
    E->getSparse();
    // Test subtree deletion
    delete C;
    ASSERT_ANY_THROW( D->getDense() );
    ASSERT_ANY_THROW( E->getSparse() );
    ASSERT_ANY_THROW( new SDR_Proxy( *E ) );
    delete D;
    // Test rest of tree is OK.
    B->getFlatSparse();
    A->zero();
    B->getFlatSparse();
    // Test delete root.
    delete A;
    ASSERT_ANY_THROW( B->getDense() );
    ASSERT_ANY_THROW( E->getSparse() );
    // Cleanup remaining Proxies.
    delete B;
    delete E;
}

TEST(SdrTest, TestProxyThrows) {
    SDR A({10});
    SDR_Proxy B(A, {2, 5});
    SDR *C = &B;

    ASSERT_ANY_THROW( C->setDense( SDR_dense_t( 10, 1 ) ));
    ASSERT_ANY_THROW( C->setSparse( SDR_sparse_t({ {0}, {0} }) ));
    ASSERT_ANY_THROW( C->setFlatSparse( SDR_flatSparse_t({ 0, 1, 2 }) ));
    ASSERT_ANY_THROW( C->setSDR( SDR({10}) ));
    ASSERT_ANY_THROW( C->randomize(.10) );
    ASSERT_ANY_THROW( C->addNoise(.10) );
}

TEST(SdrTest, TestProxyGetters) {
    SDR A({ 2, 3 });
    SDR_Proxy B( A, { 3, 2 });
    SDR *C = &B;
    // Test getting dense
    A.setDense( SDR_dense_t({ 0, 1, 0, 0, 1, 0 }) );
    ASSERT_EQ( C->getDense(), SDR_dense_t({ 0, 1, 0, 0, 1, 0 }) );

    // Test getting flat sparse
    A.setSparse( SDR_sparse_t({ {0, 1}, {0, 1} }));
    ASSERT_EQ( C->getSparse(), SDR_sparse_t({ {0, 2}, {0, 0} }) );

    // Test getting sparse
    A.setFlatSparse( SDR_flatSparse_t({ 2, 3 }));
    ASSERT_EQ( C->getFlatSparse(), SDR_flatSparse_t({ 2, 3 }) );

    // Test getting sparse, a second time.
    A.setFlatSparse( SDR_flatSparse_t({ 2, 3 }));
    ASSERT_EQ( C->getSparse(), SDR_sparse_t({ {1, 1}, {0, 1} }) );

    // Test getting sparse, when the parent SDR already has sparse computed and
    // the dimensions are the same.
    A.zero();
    SDR_Proxy D( A );
    SDR *E = &D;
    A.setSparse( SDR_sparse_t({ {0, 1}, {0, 1} }));
    ASSERT_EQ( E->getSparse(), SDR_sparse_t({ {0, 1}, {0, 1} }) );
}

/**
 * SDR_Sparsity
 * Test that it creates & destroys, and that nothing crashes.
 */
TEST(SdrTest, TestMetricSparsityConstruct) {
    SDR *A = new SDR({1});
    SDR_Sparsity S( *A, 1000u );
    ASSERT_ANY_THROW( SDR_Sparsity S( *A, 0u ) ); // Period > 0!
    A->zero();
    A->zero();
    A->zero();
    delete A; // Test use after freeing the parent SDR.
    S.min();
    S.max();
    S.mean();
    S.std();
    ASSERT_EQ( S.sparsity, 0.0f );
}

TEST(SdrTest, TestMetricSparsityExample) {
    SDR A( { 1000u } );
    SDR_Sparsity B( A, 1000u );
    A.randomize( 0.01f );
    A.randomize( 0.15f );
    A.randomize( 0.05f );
    ASSERT_EQ( B.sparsity, 0.05f);
    ASSERT_EQ( B.min(),    0.01f);
    ASSERT_EQ( B.max(),    0.15f);
    ASSERT_NEAR( B.mean(),   0.07f, 0.005f );
    ASSERT_NEAR( B.std(),    0.06f, 0.005f );
}

/*
 * SDR_Sparsity
 * Verify that the initial 10 values of metric are OK.
 */
TEST(SdrTest, TestMetricSparsityShortTerm) {
    SDR A({1});
    Real period = 10u;
    Real alpha  = 1.0f / period;
    SDR_Sparsity S( A, period );

    A.setDense(SDR_dense_t{ 1 });
    ASSERT_FLOAT_EQ( S.sparsity, 1.0f );
    ASSERT_NEAR( S.min(),  1.0f, alpha );
    ASSERT_NEAR( S.max(),  1.0f, alpha );
    ASSERT_NEAR( S.mean(), 1.0f, alpha );
    ASSERT_NEAR( S.std(),  0.0f, alpha );

    A.setDense(SDR_dense_t{ 0 });
    ASSERT_FLOAT_EQ( S.sparsity, 0.0f );
    ASSERT_NEAR( S.min(),  0.0f, alpha );
    ASSERT_NEAR( S.max(),  1.0f, alpha );
    ASSERT_NEAR( S.mean(), 1.0f / 2, alpha );
    ASSERT_NEAR( S.std(),  0.5f, alpha );

    A.setDense(SDR_dense_t{ 0 });
    ASSERT_FLOAT_EQ( S.sparsity, 0.0f );
    ASSERT_NEAR( S.min(),  0.0f, alpha );
    ASSERT_NEAR( S.max(),  1.0f, alpha );
    ASSERT_NEAR( S.mean(), 1.0f / 3, alpha );
    // Standard deviation was computed in python with numpy.std([ 1, 0, 0 ])
    ASSERT_NEAR( S.std(),  0.47140452079103168f, alpha );

    A.setDense(SDR_dense_t{ 0 });
    ASSERT_NEAR( S.mean(), 1.0f / 4, alpha );
    ASSERT_NEAR( S.std(),  0.4330127018922193f, alpha );

    A.setDense(SDR_dense_t{ 0 });
    ASSERT_NEAR( S.mean(), 1.0f / 5, alpha );
    ASSERT_NEAR( S.std(),  0.40000000000000008f, alpha );

    A.setDense(SDR_dense_t{ 0 });
    ASSERT_NEAR( S.mean(), 1.0f / 6, alpha );
    ASSERT_NEAR( S.std(),  0.372677996249965f, alpha );

    A.setDense(SDR_dense_t{ 0 });
    ASSERT_NEAR( S.mean(), 1.0f / 7, alpha );
    ASSERT_NEAR( S.std(),  0.34992710611188266f, alpha );

    A.setDense(SDR_dense_t{ 0 });
    ASSERT_NEAR( S.mean(), 1.0f / 8, alpha );
    ASSERT_NEAR( S.std(),  0.33071891388307384f, alpha );

    A.setDense(SDR_dense_t{ 0 });
    ASSERT_NEAR( S.mean(), 1.0f / 9, alpha );
    ASSERT_NEAR( S.std(),  0.31426968052735443f, alpha );

    A.setDense(SDR_dense_t{ 0 });
    ASSERT_NEAR( S.mean(), 1.0f / 10, alpha );
    ASSERT_NEAR( S.std(),  0.30000000000000004f, alpha );
}

/*
 * SDR_Sparsity
 * Verify that the longer run values of the SDR_Sparsity metric are OK.
 * Test Protocol:
 *      instantaneous-sparsity = Sample random distribution
 *      for iteration in range( 1,000 ):
 *          SDR.randomize( instantaneous-sparsity )
 *      ASSERT_NEAR( SparsityMetric.mean(), true_mean )
 *      ASSERT_NEAR( SparsityMetric.std(),  true_std )
 */
TEST(SdrTest, TestMetricSparsityLongTerm) {
    auto period     = 100u;
    auto iterations = 1000u;

    SDR A({1000u});
    SDR_Proxy B(A); // This should work.
    SDR_Sparsity S( B, period );

    vector<Real> test_means{ 0.01f,  0.05f,  0.20f, 0.50f, 0.50f, 0.75f, 0.99f };
    vector<Real> test_stdev{ 0.001f, 0.025f, 0.10f, 0.33f, 0.01f, 0.15f, 0.01f };

    std::default_random_engine generator;
    for(auto test = 0u; test < test_means.size(); test++) {
        const auto mean = test_means[test];
        const auto stdv = test_stdev[test];
        auto dist = std::normal_distribution<float>(mean, stdv);
        for(UInt i = 0; i < iterations; i++) {
            Real sparsity;
            do {
                sparsity = dist( generator );
            } while( sparsity < 0.0f || sparsity > 1.0f);
            A.randomize( sparsity );
            EXPECT_NEAR( S.sparsity, sparsity, 0.501f / A.size );
        }
        EXPECT_NEAR( S.mean(), mean, stdv );
        EXPECT_NEAR( S.std(),  stdv, stdv / 2.0f );
    }
}

TEST(SdrTest, TestMetricSparsityPrint) {
    // TODO: Automatically test.  Use regex or other parsing utility.  Extract
    // the data into real numbers and check that its acceptably near to the
    // expected.
    cerr << endl << "YOU must manually verify this output!" << endl << endl;
    SDR A({ 2000u });
    SDR_Sparsity S( A, 10u );

    A.randomize( 0.30f );
    A.randomize( 0.70f );
    S.print();

    A.randomize( 0.123456789f );
    A.randomize( 1.0f - 0.123456789f );
    S.print();
    cerr << endl;
}

/**
 * SDR_ActivationFrequency
 * Test that it creates & destroys, and that no methods crash.
 */
TEST(SdrTest, TestMetricAF_Construct) {
    // Test creating it.
    SDR *A = new SDR({ 5 });
    SDR_ActivationFrequency F( *A, 100 );
    ASSERT_ANY_THROW( SDR_ActivationFrequency F( *A, 0u ) ); // Period > 0!
    // Test nothing crashes with no data.
    F.min();
    F.mean();
    F.std();
    F.max();
    ASSERT_EQ( F.activationFrequency.size(), A->size );

    // Test with junk data.
    A->zero(); A->randomize( 0.5f ); A->randomize( 1.0f ); A->randomize( 0.5f );
    F.min();
    F.mean();
    F.std();
    F.max();
    ASSERT_EQ( F.activationFrequency.size(), A->size );

    // Test use after freeing parent SDR.
    auto A_size = A->size;
    delete A;
    F.min();
    F.mean();
    F.std();
    F.max();
    ASSERT_EQ( F.activationFrequency.size(), A_size );
}

/**
 * SDR_ActivationFrequency
 * Verify that the first few data points are ok.
 */
TEST(SdrTest, TestMetricAF_Example) {
    SDR A({ 2u });
    SDR_ActivationFrequency F( A, 10u );

    A.setDense(SDR_dense_t{ 0, 0 });
    ASSERT_EQ( F.activationFrequency, vector<Real>({ 0.0f, 0.0f }));

    A.setDense(SDR_dense_t{ 1, 1 });
    ASSERT_EQ( F.activationFrequency, vector<Real>({ 0.5f, 0.5f }));

    A.setDense(SDR_dense_t{ 0, 1 });
    ASSERT_NEAR( F.activationFrequency[0], 0.3333333333333333f, 0.001f );
    ASSERT_NEAR( F.activationFrequency[1], 0.6666666666666666f, 0.001f );
    ASSERT_EQ( F.min(), F.activationFrequency[0] );
    ASSERT_EQ( F.max(), F.activationFrequency[1] );
    ASSERT_FLOAT_EQ( F.mean(), 0.5f );
    ASSERT_NEAR( F.std(), 0.16666666666666666f, 0.001f );
    ASSERT_NEAR( F.entropy(), 0.9182958340544896f, 0.001f );
}

/*
 * SDR_ActivationFrequency
 * Verify that the longer run values of this metric are OK.
 */
TEST(SdrTest, TestMetricAF_LongTerm) {
    const auto period  =   100u;
    const auto runtime = 10000u;
    SDR A({ 20u });
    SDR_ActivationFrequency F( A, period );

    vector<Real> test_sparsity{ 0.0f, 0.02f, 0.05, 1.0f, 0.25f, 0.5f };

    for(const auto &sparsity : test_sparsity) {
        for(UInt i = 0; i < runtime; i++)
            A.randomize( sparsity );

        const auto epsilon = 0.10f;
        EXPECT_GT( F.min(), sparsity - epsilon );
        EXPECT_LT( F.max(), sparsity + epsilon );
        EXPECT_NEAR( F.mean(), sparsity, epsilon );
        EXPECT_NEAR( F.std(),  0.0f,     epsilon );
    }
}

TEST(SdrTest, TestMetricAF_Entropy) {
    const auto size    = 1000u; // Num bits in SDR.
    const auto period  =  100u; // For activation frequency exp-rolling-avg
    const auto runtime = 1000u; // Train time for each scenario
    const auto tolerance = 0.02f;

    // Extact tests:
    // Test all zeros.
    SDR A({ size });
    SDR_ActivationFrequency F( A, period );
    A.zero();
    EXPECT_FLOAT_EQ( F.entropy(), 0.0f );

    // Test all ones.
    SDR B({ size });
    SDR_ActivationFrequency G( B, period );
    B.randomize( 1.0f );
    EXPECT_FLOAT_EQ( G.entropy(), 0.0f );

    // Probabilistic tests:
    // Start with random SDRs, verify 100% entropy. Then disable cells and
    // verify that the entropy decreases.  Disable cells by freezing their value
    // so that the resulting SDR keeps the same sparsity.  Progresively disable
    // more cells and verify that the entropy monotonically decreases.  Finally
    // verify 0% entropy when all cells are disabled.
    SDR C({ size });
    SDR_ActivationFrequency H( C, period );
    auto last_entropy = -1.0f;
    const UInt incr = size / 10u; // NOTE: This MUST divide perfectly, with no remainder!
    for(auto nbits_disabled = 0u; nbits_disabled <= size; nbits_disabled += incr) {
        for(auto i = 0u; i < runtime; i++) {
            SDR scratch({size});
            scratch.randomize( 0.05f );
            // Freeze bits, such that nbits remain alive.
            for(auto z = 0u; z < nbits_disabled; z++)
                scratch.getDense()[z] = C.getDense()[z];
            C.setDense( scratch.getDense() );
        }
        const auto entropy = H.entropy();
        if( nbits_disabled == 0u ) {
            // Expect 100% entropy
            EXPECT_GT( entropy, 1.0f - tolerance );
        }
        else{
            // Expect less entropy than last time, when fewer bits were disabled.
            ASSERT_LT( entropy, last_entropy );
        }
        last_entropy = entropy;
    }
    // Expect 0% entropy.
    EXPECT_LT( last_entropy, tolerance );
}

TEST(SdrTest, TestMetricAF_Print) {
    // TODO: Automatically test.  Use regex or other parsing utility.  Extract
    // the data into real numbers and check that its acceptably near to the
    // expected.
    const auto period  =  100u;
    const auto runtime = 1000u;
    cerr << endl << "YOU must manually verify this output!" << endl << endl;
    SDR A({ 2000u });
    SDR_ActivationFrequency F( A, period );

    vector<Real> sparsity{ 0.0f, 0.02f, 0.05f, 0.50f, 0.0f };
    for(const auto sp : sparsity) {
        for(auto i = 0u; i < runtime; i++)
            A.randomize( sp );
        F.print();
        cerr << endl;
    }
}

TEST(SdrTest, TestMetricOverlap_Construct) {
    SDR *A = new SDR({ 1000u });
    SDR_Overlap V( *A, 100u );
    ASSERT_ANY_THROW( new SDR_Overlap( *A, 0 ) ); // Period > 0!
    // Check that it doesn't crash, when uninitialized.
    V.min();
    V.mean();
    V.std();
    V.max();
    // If no data, have obviously wrong result.
    ASSERT_FALSE( V.overlap >= 0.0f and V.overlap <= 1.0f );

    // Check that it doesn't crash with half enough data.
    A->randomize( 0.20f );
    V.min();
    V.mean();
    V.std();
    V.max();
    ASSERT_FALSE( V.overlap >= 0.0f and V.overlap <= 1.0f );

    // Check no crash with data.
    A->addNoise( 0.50f );
    V.min();
    V.mean();
    V.std();
    V.max();
    ASSERT_EQ( V.overlap, 0.50f );

    // Check overlap metric is valid after parent SDR is deleted.
    delete A;
    V.min();
    V.mean();
    V.std();
    V.max();
    ASSERT_EQ( V.overlap, 0.50f );
}

TEST(SdrTest, TestMetricOverlap_Example) {
    SDR A({ 10000u });
    SDR_Overlap B( A, 1000u );
    A.randomize( 0.05 );
    A.addNoise( 0.95 );         //   5% overlap
    A.addNoise( 0.55 );         //  45% overlap
    A.addNoise( 0.72 );         //  28% overlap
    ASSERT_EQ( B.overlap,  0.28f );
    ASSERT_EQ( B.min(),    0.05f );
    ASSERT_EQ( B.max(),    0.45f );
    ASSERT_NEAR( B.mean(), 0.26f, 0.005f );
    ASSERT_NEAR( B.std(),  0.16f, 0.005f );
}

TEST(SdrTest, TestMetricOverlap_ShortTerm) {
    SDR         A({ 1000u });
    SDR_Overlap V( A, 10u );

    A.randomize( 0.20f ); // Initial value is taken after SDR_Overlap is created

    // Add overlap 50% to metric tracker.
    A.addNoise(  0.50f );
    ASSERT_FLOAT_EQ( V.overlap, 0.50f );
    ASSERT_FLOAT_EQ( V.min(),   0.50f );
    ASSERT_FLOAT_EQ( V.max(),   0.50f );
    ASSERT_FLOAT_EQ( V.mean(),  0.50f );
    ASSERT_FLOAT_EQ( V.std(),   0.0f );

    // Add overlap 80% to metric tracker.
    A.addNoise(  0.20f );
    ASSERT_FLOAT_EQ( V.overlap, 0.80f );
    ASSERT_FLOAT_EQ( V.min(),   0.50f );
    ASSERT_FLOAT_EQ( V.max(),   0.80f );
    ASSERT_FLOAT_EQ( V.mean(),  0.65f );
    ASSERT_FLOAT_EQ( V.std(),   0.15f );

    // Add overlap 25% to metric tracker.
    A.addNoise(  0.75f );
    ASSERT_FLOAT_EQ( V.overlap, 0.25f );
    ASSERT_FLOAT_EQ( V.min(),   0.25f );
    ASSERT_FLOAT_EQ( V.max(),   0.80f );
    ASSERT_FLOAT_EQ( V.mean(),  0.51666666666666672f ); // Source: python numpy.mean
    ASSERT_FLOAT_EQ( V.std(),   0.22484562605386735f ); // Source: python numpy.std
}

TEST(SdrTest, TestMetricOverlap_LongTerm) {
    const auto runtime = 1000u;
    const auto period  =  100u;
    SDR A({ 500u });
    SDR_Overlap V( A, period );
    A.randomize( 0.45f );

    vector<Real> mean_ovlp{ 0.0f, 1.0f,
                            0.5f, 0.25f,
                            0.85f, 0.95f };

    vector<Real> std_ovlp{  0.01f, 0.01f,
                            0.33f, 0.05f,
                            0.05f, 0.02f };

    std::default_random_engine generator;
    for(auto i = 0u; i < mean_ovlp.size(); i++) {
        auto dist = std::normal_distribution<float>(mean_ovlp[i], std_ovlp[i]);

        for(auto z = 0u; z < runtime; z++) {
            Real ovlp;
            do {
                ovlp = dist( generator );
            } while( ovlp < 0.0f || ovlp > 1.0f );
            A.addNoise( 1.0f - ovlp );
            EXPECT_NEAR( V.overlap, ovlp, 0.501f / A.getSum() );
        }
        EXPECT_NEAR( V.mean(), mean_ovlp[i], std_ovlp[i] );
        EXPECT_NEAR( V.std(),  std_ovlp[i],  std_ovlp[i] / 2.0f );
    }
}

TEST(SdrTest, TestMetricOverlap_Print) {
    // TODO: Automatically test.  Use regex or other parsing utility.  Extract
    // the data into real numbers and check that its acceptably near to the
    // expected.
    cerr << endl << "YOU must manually verify this output!" << endl << endl;
    SDR A({ 2000u });
    SDR_Overlap V( A, 100u );
    A.randomize( 0.02f );

    vector<Real> overlaps{ 0.02f, 0.05f, 0.0f, 0.50f, 0.0f };
    for(const auto ovlp : overlaps) {
        for(auto i = 0u; i < 1000u; i++)
            A.addNoise( 1.0f - ovlp );
        V.print();
    }
    for(auto i = 0u; i < 1000u; i++)
        A.randomize( 0.02f );
    V.print();
    cerr << endl;
}

/**
 * SDR_Metrics
 *
 */
TEST(SdrTest, TestAllMetrics_Construct) {
    // Test that it constructs.
    SDR *A = new SDR({ 100u });
    SDR_Metrics M( *A, 10u );

    A->randomize( 0.05f );
    A->randomize( 0.05f );
    A->randomize( 0.05f );

    // Test use after freeing data source.
    delete A;
    stringstream devnull;
    M.print( devnull );

    // Test delete Metric and keep using SDR.
    A = new SDR({ 100u });
    SDR_Metrics *B = new SDR_Metrics( *A, 99u );
    SDR_Metrics *C = new SDR_Metrics( *A, 98u );
    A->randomize( 0.20f );
    A->randomize( 0.20f );
    B->print( devnull );
    delete B;                   // First deletion
    A->randomize( 0.20f );
    A->addNoise( 0.20f );
    B = new SDR_Metrics( *A, 99u );    // Remove & Recreate
    A->randomize( 0.20f );
    A->randomize( 0.20f );
    A->print( devnull );
    delete A;
    C->print( devnull );
    delete C;
    B->print( devnull );
    delete B;
}

/**
 * SDR_Metrics prints OK.
 */
TEST(SdrTest, TestAllMetrics_Print) {
    // TODO: Automatically test.  Use regex or other parsing utility.  Extract
    // the data into real numbers and check that its acceptably near to the
    // expected.
    cerr << endl << "YOU must manually verify this output!" << endl << endl;
    SDR A({ 4097u });
    SDR_Metrics M( A, 100u );

    vector<Real> sparsity{ 0.02f, 0.15f, 0.06f, 0.50f, 0.0f };
    vector<Real> overlaps{ 0.02f, 0.05f, 0.10f, 0.50f, 0.0f };
    for(auto test = 0u; test < sparsity.size(); test++) {
        A.randomize( sparsity[test] );
        for(auto i = 0u; i < 1000u; i++)
            A.addNoise( 1.0f - overlaps[test] );
        M.print();
        cerr << endl;
    }
}

