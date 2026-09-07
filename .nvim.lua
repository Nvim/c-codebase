vim.filetype.add({
  extension = {
    h = "c",
  },
})

-- vim.o.makeprg = "cmake --build build && ctest --test-dir build --output-on-failure"
vim.o.makeprg = "cmake --workflow test-clang"
vim.o.errorformat = '%f:%l:%.%#:FAIL%.%m'
