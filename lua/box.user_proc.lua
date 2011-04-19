require("fiber")
require("box")

local error, print, pairs, type = error, print, pairs, type
local table, box = table, box

module(...)

local def = box.defproc

def("get_first_tuple",
    function (txn, namespace, key)
            local index = namespace.index[0]
            local tuple = index[key]
            if tuple then
                    return {tuple, tuple, tuple}
            end
            return nil
    end)


def("get_all_tuples", 
    function (txn, namespace)
            local index = namespace.index[0]
            local result = {}
            for i, tuple in box.index.hashpairs(index) do
                    table.insert(result, tuple)
            end
            return 0, result
    end)