-- Signed division and statement-only /= exercise runtime fault sites.
function division(a: i32, b: i32, c: i32): i32
  local quotient: i32 = a / b
  quotient /= c
  return quotient
end
