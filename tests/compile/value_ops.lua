-- Constant-left subtraction, register multiply, negation, and dead folding.
function value_ops(a: i32, b: i32, c: i32): i32
  return ((17 - a) * b) + -(c * a) + (2 + 3) * 0
end
