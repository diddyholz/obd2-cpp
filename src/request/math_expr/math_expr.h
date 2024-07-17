#pragma once

#include <memory>
#include <string>
#include <vector>

namespace obd2 {
    class math_expr {
        private:
            enum math_op {
                ADDITION = '+',
                SUBTRACTION = '-',
                MULTIPLICATION = '*',
                DIVISION = '/',
                EXPONENTIATION = '^',
                VARIABLE,
                RAW
            };

            math_op operation;
            
            std::unique_ptr<math_expr> left;
            std::unique_ptr<math_expr> right;
            
            size_t value_index;
            uint8_t value_mask;
            uint8_t value_shift;
            
            float value_raw;

            void optimize_raw();
            bool parse_raw(const std::string &formula);
            bool parse_variable(const std::string &formula);
            size_t find_operator(const std::string &formula) const;
            size_t find_outside(const std::string &str, char c, size_t l, size_t r) const;
            std::string strip_parentheses(const std::string &formula);

        public:
            math_expr();
            math_expr(const std::string &formula);
            math_expr(const math_expr &e);
            math_expr(math_expr &&e);

            math_expr &operator=(math_expr &e);
            math_expr &operator=(math_expr &&e);

            float solve(const std::vector<uint8_t> &input_values);
    };
}