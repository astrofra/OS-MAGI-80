-- Initial typed-expression compiler fixture.
function arithmetic(a: i32, b: i32, c: i32): i32
  return (a * 3 + b) - (a + -5) + c * 2
end
