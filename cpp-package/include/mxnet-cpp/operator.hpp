/*!
*  Copyright (c) 2016 by Contributors
* \file operator.hpp
* \brief implementation of operator
* \author Chuntao Hong, Zhang Chen
*/

#ifndef MXNETCPP_OPERATOR_HPP
#define MXNETCPP_OPERATOR_HPP

#include <algorithm>
#include <string>
#include <vector>
#include <iterator>
#include "mxnet-cpp/base.h"
#include "mxnet-cpp/op_map.h"
#include "mxnet-cpp/operator.h"

namespace mxnet {
namespace cpp {

/*
 * Pushing NDArray or Symbol as inputs here to avoid partial specialization
 * like PushInput<NDArray, Args..., N>, which is not allowed in C++
 */
template <>
inline Operator& Operator::SetParam<NDArray>(int pos, const NDArray &value) {
  input_ndarrays.push_back(value.GetHandle());
  return *this;
}
template <>
inline Operator& Operator::SetParam<Symbol>(int pos, const Symbol &value) {
  input_symbols.push_back(value.GetHandle());
  return *this;
}

inline OpMap*& Operator::op_map() {
  static OpMap *op_map_ = new OpMap();
  return op_map_;
}

inline Operator::Operator(const std::string &operator_name) {
  handle_ = op_map()->GetSymbolCreator(operator_name);
  const char *name;
  const char *description;
  mx_uint num_args;
  const char **arg_names;
  const char **arg_type_infos;
  const char **arg_descriptions;
  const char *key_var_num_args;
  MXSymbolGetAtomicSymbolInfo(handle_,
      &name,
      &description,
      &num_args,
      &arg_names,
      &arg_type_infos,
      &arg_descriptions,
      &key_var_num_args);
  for (mx_uint i = 0; i < num_args; ++i) {
    arg_names_.push_back(arg_names[i]);
  }
}

inline Symbol Operator::CreateSymbol(const std::string &name) {
  if (input_keys.size() > 0) {
    CHECK_EQ(input_keys.size(), input_symbols.size());
  }
  const char *pname = name == "" ? nullptr : name.c_str();

  SymbolHandle symbol_handle;
  std::vector<const char *> input_keys;
  std::vector<const char *> param_keys;
  std::vector<const char *> param_values;

  for (auto &data : params_) {
    param_keys.push_back(data.first.c_str());
    param_values.push_back(data.second.c_str());
  }
  for (auto &data : this->input_keys) {
    input_keys.push_back(data.c_str());
  }
  const char **input_keys_p =
      (input_keys.size() > 0) ? input_keys.data() : nullptr;

  MXSymbolCreateAtomicSymbol(handle_, param_keys.size(), param_keys.data(),
                             param_values.data(), &symbol_handle);
  MXSymbolCompose(symbol_handle, pname, input_symbols.size(), input_keys_p,
                  input_symbols.data());
  return Symbol(symbol_handle);
}

inline void Operator::Invoke(std::vector<NDArray> &outputs) {
  if (input_keys.size() > 0) {
    CHECK_EQ(input_keys.size(), input_ndarrays.size());
  }

  std::vector<const char *> input_keys;
  std::vector<const char *> param_keys;
  std::vector<const char *> param_values;

  for (auto &data : params_) {
    param_keys.push_back(data.first.c_str());
    param_values.push_back(data.second.c_str());
  }

  int num_inputs = input_ndarrays.size();
  int num_outputs = outputs.size();
  std::vector<NDArrayHandle> output_handles;
  std::transform(outputs.begin(), outputs.end(),
      std::back_inserter(output_handles), [](NDArray& a) {
        return a.GetHandle();
      });

  NDArrayHandle *outputs_receiver = nullptr;
  if (num_outputs > 0) {
    outputs_receiver = output_handles.data();
  }

  MXImperativeInvoke(handle_, num_inputs, input_ndarrays.data(),
      &num_outputs, &outputs_receiver,
      param_keys.size(), param_keys.data(), param_values.data());

  if (outputs.size() > 0)
    return;

  std::transform(outputs_receiver, outputs_receiver+num_outputs,
      std::back_inserter(outputs), [](const NDArrayHandle& handle) {
        return NDArray(handle);
      });
}

inline std::vector<NDArray> Operator::Invoke() {
  std::vector<NDArray> outputs;
  Invoke(outputs);
  return outputs;
}

inline void Operator::Invoke(NDArray &output) {
  std::vector<NDArray> outputs{output};
  Invoke(outputs);
}

inline Operator &Operator::SetInput(const std::string &name, Symbol symbol) {
  input_keys.push_back(name.c_str());
  input_symbols.push_back(symbol.GetHandle());
  return *this;
}

inline Operator &Operator::SetInput(const std::string &name, NDArray ndarray) {
  input_keys.push_back(name.c_str());
  input_ndarrays.push_back(ndarray.GetHandle());
  return *this;
}

}  // namespace cpp
}  // namespace mxnet

#endif  // MXNETCPP_OPERATOR_HPP
