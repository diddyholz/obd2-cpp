#include "math_expr.h"

#include <cstdlib>
#include <cctype>
#include <exception>
#include <limits>
#include <stdexcept>

namespace obd2 {
    math_expr::math_expr(const std::string &formula) {
        if (formula == "") {
            value_raw = 0.0;
            operation = RAW;
            return;
        }

        std::string tmp_formula = strip_parentheses(formula);
        size_t op_pos = find_operator(tmp_formula);
        
        // Check if operator was found
        if (op_pos != std::string::npos) {
            operation = math_op(tmp_formula[op_pos]);
            left = new math_expr(tmp_formula.substr(0, op_pos));
            size_t len = tmp_formula.size() - (op_pos + 1);
            right = new math_expr(tmp_formula.substr(op_pos + 1, len));
            
            return;
        }

        if (parse_raw(tmp_formula)) {
            return;
        }

        if (parse_variable(tmp_formula)) {
            return;
        }

        throw std::invalid_argument("Invalid usage of variable or raw value");
    }

    math_expr::math_expr(const math_expr &e) 
        : operation(e.operation), value_index(e.value_index), value_mask(e.value_mask) {
        if (e.left) {
            left = new math_expr(*(e.left));
        }

        if (e.right) {
            right = new math_expr(*(e.right));
        }
    }

    math_expr::math_expr(math_expr &&e) 
        : operation(e.operation), left(e.left), right(e.right), value_index(e.value_index), value_mask(e.value_mask) { }

    math_expr::~math_expr() {
        delete left;
        delete right;
    }

    math_expr &math_expr::operator=(math_expr &e) {
        delete left;
        delete right;

        if (e.left) {
            left = new math_expr(*(e.left));
        }
        else {
            left = nullptr;
        }

        if (e.right) {
            right = new math_expr(*(e.right));
        }
        else {
            right = nullptr;
        }

        operation = e.operation;
        value_index = e.value_index;
        value_mask = e.value_mask;
        value_mask = e.value_mask;
        
        return *this;
    }

    float math_expr::solve(const std::vector<uint8_t> &input_values) {
        float left_val = 0.0;
        float right_val = 0.0;

        if (left && right) {
            left_val = left->solve(input_values);
            right_val = right->solve(input_values);
        }

        switch (operation)
        {
            case ADDITION:
                return left_val + right_val;
            case SUBTRACTION:
                return left_val - right_val;
            case MULTIPLICATION:
                return left_val * right_val;
            case DIVISION:
                if (right_val == 0) {
                    return std::numeric_limits<float>::infinity();
                }

                return left_val / right_val;
            case VARIABLE:
                if (value_index >= input_values.size()) {
                    return 0.0;
                }

                return (input_values[value_index] & value_mask) >> value_shift;
            case RAW:
                return value_raw;

            default:
                return 0.0;
        }
    }

    size_t math_expr::find_operator(const std::string &formula) const {
        const size_t npos = std::string::npos;
        std::vector<std::string> vector;
        
        size_t l_bracket = formula.find('(');
        size_t r_bracket = formula.find_last_of(')');

        if ((l_bracket == npos || r_bracket == npos) && l_bracket != r_bracket) {
            throw std::invalid_argument("Invalid usage of parantheses in formula");
        }

        // Set default boundaries if no bracket is found
        if (l_bracket == npos) {
            l_bracket = SIZE_MAX;
            r_bracket = 0;
        }

        size_t add = find_outside(formula, '+', l_bracket, r_bracket);
        size_t sub = find_outside(formula, '-', l_bracket, r_bracket);

        if (add != npos && (add > sub || sub == npos)) {
            return add;
        }
        else if (sub != npos && (sub > add || add == npos)) {
            return sub;
        }

        size_t mul = find_outside(formula, '*', l_bracket, r_bracket);
        size_t div = find_outside(formula, '/', l_bracket, r_bracket);

        if (mul != npos && (mul > div || div == npos)) {
            return mul;
        }
        else if (div != npos && (div > mul || mul == npos)) {
            return div;
        }

        size_t exp = find_outside(formula, '^', l_bracket, r_bracket);

        if (exp != npos) {
            return exp;
        }

        return npos;
    }

    size_t math_expr::find_outside(const std::string &str, char c, size_t l, size_t r) const {
        size_t pos = str.find(c);

        if (pos < l) {
            return pos;
        }
        
        return str.find(c, r + 1);
    }

    bool math_expr::parse_raw(const std::string &formula) {
        char *endptr = nullptr;
        float val = std::strtof(formula.c_str(), &endptr);

        if (*endptr == '\0' || isspace(*endptr)) {
            operation = RAW;
            value_raw = val;
            return true;
        }

        return false;
    }

    bool math_expr::parse_variable(const std::string &formula) {
        if (formula.size() > 2 || formula.size() == 0) {
            return false;
        }

        if (!isalpha(formula[0])) {
            return false;
        }
        
        operation = VARIABLE;
        value_index = static_cast<size_t>(tolower(formula[0]) - 'a');
        value_mask = 0xFF;
        value_shift = 0;

        if (formula.size() == 1) {
            return true;
        }

        if (!isdigit(formula[1]) || formula[1] > 7) {
            return false;
        }

        value_shift = static_cast<uint8_t>(formula[1]);
        value_mask = 1 << value_shift;

        return true;
    }

    std::string math_expr::strip_parentheses(const std::string &formula) {
        std::string tmp = formula;

        while (tmp[0] == '(' && tmp[tmp.size() - 1] == ')'){
            tmp = tmp.substr(1, tmp.size() - 2);
        }

        return tmp;
    }
}
