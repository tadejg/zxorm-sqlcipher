# ZxORM
**Z**ach's **O**bject **R**elational **M**apping library - A C++20 ORM for SQLite
___

`zxorm` is an attempt to create an ORM for C++ that achieves the following:
* Compile time type safety
* No code generation
* Low amount of boilerplate code
* No requirement to inherit from anything or conform to a prescribed interface
* SQL like syntax for writing more complicated queries without writing raw SQL


## Getting started
1. Write your objects

```cpp
struct Object {
    int id = 0;
    std::string some_text;
};

```
`zxorm` doesn't provide a base class or anything like that. The types for the
SQL table are instead inferred from types used in the C++ code.

2. Create a `Table` for your object
```cpp
using ObjectTable = zxorm::Table<"objects", Object,
    zxorm::Column<"id", &Object::id, zxorm::PrimaryKey<>>,
    zxorm::Column<"some_text", &Object::some_text>
>;
```
This is the mapping that will tell `zxorm` how to deal with `Object` and what table
in the database it is referring to.

3. Create a connection
```cpp
auto result = zxorm::Connection<ObjectTable>::create("data.db");
assert(!result.is_error());
auto connection = std::move(result.value());
```
The connection template accepts a list of tables, and should contain all the tables
that your application is going to work with.

4. Start running queries
```cpp
connection.create_tables();
connection.insert_many_records(std::vector<Object> {
    { .some_text = "This is my data" },
    { .some_text = "Wow it is so important" },
    { .some_text = "Better make sure it is saved" },
    { .some_text = "!" },
});
auto result = connection.find_record<Object>(4);
assert(!result.is_error() && result.has_value());
assert(result.value().some_text == "!");
```

Check out the [example](./example.cpp) for more
___
## Building
The library is header only, so all you need to do is include the `includes` directory,
and link sqlite
```sh
g++ example.cpp -Izxorm/includes -o example.bin `pkg-config --libs sqlite3` -std=c++20
```

___
There is also a `CMakeLists.txt` that can be used for integrating easily into a
CMake project. Simply include this repository as a subdirectory:
```cmake
add_subdirectory("./zxorm")
```

Currently only `gcc` 12 and `clang` 15 are tested and working on linux

I don't have a Windows machine, and won't be adding support any time soon.
___
## Usage

### Select queries

For more complicated queries, the connection class has the `select_query`.

This function returns a query object that can be executed (using `many` or `one`)
to obtain results.

e.g.
```cpp
auto results = connection.select_query<Object>().many();
// or
auto result = connection.select_query<Object>().one();
```

#### Where

A `where` clause can also be added like so:
```cpp
auto results = connection.select_query<Object>()
    .where(ObjectTable::field_t<"some_text">().like("hello %"))
    .many();
```
In order to reference specific fields on the table, the `Table` template must be
used (here `ObjectTable` is the alias from the example at the top of the README).

#### Limits

Similarly, `order` and `limits` can be applied
```cpp
auto results = connection.select_query<Object>()
    .order_by<ObjectTable::field_t<"some_text">>(zxorm::order_t::DESC)
    .limit(10)
    .many();
```

#### Selecting specific columns:

The same field template can be used to select specific columns too:
```cpp
auto results = connection.select_query<ObjectTable::field_t<"some_text">>().many();
```

Multiple items can be selected using the `Select` template.
The results will be returned as tuples

```cpp
zxorm::Result<std::tuple<int, Object>> result = connection.select_query<
    Select<ObjectTable::field_t<"id">, Object>
>().one();
```

#### Joins

The template arguments for the `select_query` function use an SQL-like syntax
and can include which columns or tables to return, as well as other tables that
should be joined like so:
```cpp
auto results = connection.select_query<
    Select<User, UserData>,
    From<User>,
    Join<UserData>
>().many();

// In case you are interested, the type of `results` is:
// zxorm::Result<zxorm::RecordIterator<std::tuple<User, UserData>>>
```
If the `From` clause is omitted, then it will default to the first thing selected

Of course, many joins can be used. The only limitation is on the order that the
clauses are used. Each join should refer to a table that was already "joined"
in a previous clause:
```cpp
auto results = connection.select_query<
    Select<User, Group>,
    From<User>,
    Join<UserGroup>, // we can imagine this is a join table, joining users & groups
    Join<Group>
>().many();
```

The `Join` template will only work if `zxorm` knows about a foreign key that can
be used to relate the two tables.

If there is no foreign key, the `JoinOn` template can be used instead, which takes
two `zxorm::Field`s instead e.g.

```cpp
auto results = connection.select_query<
    Select<User, Group>,
    From<User>,
    JoinOn<UserGroupTable::field_t<"user_id">, UserTable::field_t<"id">>,
    JoinOn<GroupTable::field_t<"id">, UserGroupTable::field_t<"group_id">>
>().many();
```
_The order of the two fields doesn't matter_


#### Counting & Grouping

The `Count` template can be used instead of a table or column to select a count
```cpp
// you can specify the column to count:
auto result = my_conn->select_query<Count<ObjectTable::field_t<"id">>().one();

// if unspecified, the primary key will be used
auto result = my_conn->select_query<Count<ObjectTable>>().one();
```

`CountAll` can be used to select `COUNT(*)` also
```cpp
auto result = my_conn->select_query<CountAll, From<Object>>().one();
```

And of course the results can be grouped too, allowing occurrences to be counted:
```cpp
auto results = my_conn->select_query<
    Select<CountAll, ObjectTable::field_t<"some_text">>,
    From<Object>
>().group_by<ObjectTable::field_t<"some_text">>();
```
___
### Delete queries

The connection has a `delete_query` function that can be used to construct and
execute deletions. It behaves similarly to the `select_query` interface, although
it is much simpler.

```cpp
auto err = my_conn->delete_query<Object>()
    .where(ObjectTable::field_t<"some_text">().like("hello %")
    .exec();
```

Using joins in a delete query is not supported, since it is not part of the SQL
standard, and not supported by SQLite.
___
### Caching queries

The basic queries such as `find_record`, `insert_record` and `delete_record` will
use statement caching, meaning that the query string does not need to be regenerated,
and the statement doesn't need to be recompiled by the underlying SQL engine.

This is possible since the shape of these queries, and the number of binds
never changes. For more open ended queries, caching and reuse is possible,
but it is up to **you** to implement.

It is very important to note that when reusing queries, it is not possible to
change the text of a query that was already executed, it can only be bound with
different parameters. If a query is reused with a different where clause,
then undefined behaviour may occur.
___

### Multithreading

This is currently not well tested but in theory it should work fine as long as
you follow the golden rule:

**:warning: 1 connection per thread**

___
## Why did I write this?

I created this library after looking at what was available in C++ and seeing that
the options for a true ORM library are quite limited.

Many C++ programmers will tell you that an ORM is simply bloat that will slow
your performance, and they're not entirely wrong. However, there is a reason
why they proliferate in higher level languages: they are incredibly valuable
for the maintainability of large projects. `zxorm` is an attempt to have our cake
and eat it.

Almost all C++ ORMs are built using traditional object oriented paradigms, without
making much use of modern C++ metaprogramming techniques. C++20 introduced many
useful metaprogramming features into the language, which `zxorm` utilizes to
generate queries with compile-time type checking.

Much influence was taken from the excellent [`sqlpp11`](https://github.com/rbock/sqlpp11)
library, however it is not a true ORM, and requires code generation, or manually
writing a lot of boilerplate.

I wanted to write something that is simple to integrate, easy to start using, and
totally agnostic to how the `Object` in the `ORM` is written.

___
