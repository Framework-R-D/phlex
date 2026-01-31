{
  event_product(product)::
    {
      creator: 'input',
      suffix: product,
      layer: 'event',
    },

  creator_event_product(creator, product)::
    {
      creator: creator,
      suffix: product,
      layer: 'event',
    },
}
