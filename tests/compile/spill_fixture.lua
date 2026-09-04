-- Semantic oracle for the deliberately pressure-heavy value-IR fixture.
function spill_fixture(a: i32, b: i32, c: i32): i32
  return (a * b) * 5 + a + c
end
