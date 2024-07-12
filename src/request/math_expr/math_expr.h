#pragma once

#include <string>
#include <vector>

namespace obd2 {
    enum math_op {
        ADDITION = '+',
        SUBTRACTION = '-',
        MULTIPLICATION = '*',
        DIVISION = '/',
        VARIABLE,
        RAW
    };

    class math_expr {
        private:
            math_op operation;
            
            math_expr *left = nullptr;
            math_expr *right = nullptr;
            
            size_t value_index;
            uint8_t value_mask;
            uint8_t value_shift;
            
            float value_raw;

            bool parse_raw(const std::string &formula);
            bool parse_variable(const std::string &formula);
            size_t find_operator(const std::string &formula) const;
            size_t find_outside(const std::string &str, char c, size_t l, size_t r) const;
            std::string strip_parentheses(const std::string &formula);

        public:
            math_expr(const std::string &formula);
            math_expr(const math_expr &e);
            math_expr(math_expr &&e);
            ~math_expr();

            math_expr &operator=(math_expr &e);

            float solve(const std::vector<uint8_t> &input_values);
    };
}