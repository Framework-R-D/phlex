# FORM (Fine-grained Object Reading/Writing Model)

Prototype development for I/O infrastructure supporting Phlex

## Product suffix text

The Phlex FORM writer copies the textual value of each product specification's
`suffix_t` into FORM's product label. An explicit empty suffix (`empty_string{}`)
is written as `""`; a named suffix is written with its original spelling, including
case and underscores (for example, `Hits_2`). Empty text is a concrete product
suffix, not an omitted product or an unrestricted selector.

The persisted representation is unchanged: the product dictionary retains that
suffix text in `product_name`, and its `container_name` remains `row_space/suffix`.
An empty suffix therefore retains the trailing `/`, while a named suffix retains
its text after `/`. No placeholder name or file-format migration is introduced.

## build

`cmake -DUSE_FORM_ALONE=ON -DFORM_USE_ROOT_STORAGE=ON ../phlex/ ; make`

## run writer

`./test/form/phlex_writer ; ls -l toy.root`

## ROOT checks

```cpp
TFile* file =  TFile::Open("toy.root")
file->ls()
TTree* tree1 = (TTree*)file->Get("<ToyAlg_Segment>");
tree1->Print()
TTree* tree2 = (TTree*)file->Get("<ToyAlg_Event>");
tree2->Print()
tree1->Scan()
tree2->Scan()
```

## run reader

`./test/form/phlex_reader`
