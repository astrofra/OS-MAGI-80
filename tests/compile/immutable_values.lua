-- Canonical immutable strings and interned symbols across CFG joins.
function immutable_values(flag: bool): bool
  local text: string = "same\n"
  local key: symbol = symbol("hero")
  if flag then
    text = 'same\x0a'
    key = symbol('hero')
  else
    text = "other"
    key = symbol("enemy")
  end
  return (text == "same\n") == (key == symbol("hero"))
end
