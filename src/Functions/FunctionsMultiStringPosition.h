#pragma once

#include <Columns/ColumnArray.h>
#include <Columns/ColumnConst.h>
#include <Columns/ColumnString.h>
#include <Columns/ColumnFixedString.h>
#include <Columns/ColumnVector.h>
#include <DataTypes/DataTypeArray.h>
#include <DataTypes/DataTypeString.h>
#include <DataTypes/DataTypesNumber.h>
#include <Functions/FunctionHelpers.h>
#include <Functions/IFunction.h>
#include <IO/WriteHelpers.h>
#include <Interpreters/Context.h>
#include <base/StringRef.h>


namespace DB
{
/**
  * multiSearchAllPositions(haystack, [pattern_1, pattern_2, ..., pattern_n]) -- find first occurrences (positions) of all the const patterns inside haystack
  * multiSearchAllPositionsUTF8(haystack, [pattern_1, pattern_2, ..., pattern_n])
  * multiSearchAllPositionsCaseInsensitive(haystack, [pattern_1, pattern_2, ..., pattern_n])
  * multiSearchAllPositionsCaseInsensitiveUTF8(haystack, [pattern_1, pattern_2, ..., pattern_n])
  */

namespace ErrorCodes
{
    extern const int ILLEGAL_TYPE_OF_ARGUMENT;
    extern const int ILLEGAL_COLUMN;
}


template <typename Impl>
class FunctionsMultiStringPosition : public IFunction
{
public:
    static constexpr auto name = Impl::name;

    static FunctionPtr create(ContextPtr) { return std::make_shared<FunctionsMultiStringPosition>(); }
    String getName() const override { return name; }
    size_t getNumberOfArguments() const override { return 2; }
    bool useDefaultImplementationForConstants() const override { return true; }
    ColumnNumbers getArgumentsThatAreAlwaysConstant() const override { return {1}; }
    bool isSuitableForShortCircuitArgumentsExecution(const DataTypesWithConstInfo & /*arguments*/) const override { return true; }

    DataTypePtr getReturnTypeImpl(const DataTypes & arguments) const override
    {
        if (!isString(arguments[0]))
            throw Exception(
                ErrorCodes::ILLEGAL_TYPE_OF_ARGUMENT,
                "Illegal type {} of argument of function {}", arguments[0]->getName(), getName());

        const DataTypeArray * array_type = checkAndGetDataType<DataTypeArray>(arguments[1].get());
        if (!array_type || !checkAndGetDataType<DataTypeString>(array_type->getNestedType().get()))
            throw Exception(
                ErrorCodes::ILLEGAL_TYPE_OF_ARGUMENT,
                "Illegal type {} of argument of function {}", arguments[1]->getName(), getName());

        return std::make_shared<DataTypeArray>(std::make_shared<DataTypeUInt64>());
    }

    ColumnPtr executeImpl(const ColumnsWithTypeAndName & arguments, const DataTypePtr &, size_t /*input_rows_count*/) const override
    {
        const ColumnPtr & haystack_ptr = arguments[0].column;
        const ColumnPtr & needles_ptr = arguments[1].column;

        const ColumnString * col_haystack_vector = checkAndGetColumn<ColumnString>(&*haystack_ptr);

        const ColumnConst * col_needles_const = checkAndGetColumnConst<ColumnArray>(needles_ptr.get());

        if (!col_needles_const)
            throw Exception(
                ErrorCodes::ILLEGAL_COLUMN,
                "Illegal column {}. The array is not const", arguments[1].column->getName());

        const size_t column_haystack_size = haystack_ptr->size();

        using ResultType = typename Impl::ResultType;
        auto col_res = ColumnVector<ResultType>::create();
        auto col_offsets = ColumnArray::ColumnOffsets::create(column_haystack_size);

        auto & vec_res = col_res->getData();
        auto & offsets_res = col_offsets->getData();
        /// the implementations are responsible for resizing the output column

        if (col_haystack_vector)
            Impl::vectorConstant(
                col_haystack_vector->getChars(), col_haystack_vector->getOffsets(),
                /// needles,
                col_needles_const->getValue<Array>(),
                vec_res, offsets_res);
        else
            throw Exception(
                    ErrorCodes::ILLEGAL_COLUMN,
                    "Illegal column {}", arguments[0].column->getName());

        return ColumnArray::create(std::move(col_res), std::move(col_offsets));
    }
};

}
