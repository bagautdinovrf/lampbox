/* One Tide composition, six materials, sixteen interchangeable palettes. */
globalThis.MBCatalog = {
  variants: [
    {id:'tide',name:'Прилив',short:'Оригинал',palette:'tide',description:'Исходный вариант: широкое расписание, каналы слева и медиатека внизу.',material:'Чистые поверхности'},
    {id:'tide-relief',name:'Прилив · Рельеф',short:'Рельеф',palette:'pearl',description:'Мягкий объём, выпуклые кнопки и утопленные поля. Тактильная светлая панель.',material:'Светлый неоморфизм'},
    {id:'tide-glass',name:'Прилив · Стекло',short:'Стекло',palette:'prism',description:'Полупрозрачные слои, светлые кромки и капсульные элементы управления.',material:'Матовое стекло'},
    {id:'tide-signal',name:'Прилив · Сигнал',short:'Сигнал',palette:'lagoon',description:'Чёткие рамки, точные шкалы и графичные контролы с цветным акцентом.',material:'Точная графика'},
    {id:'tide-console',name:'Прилив · Пульт',short:'Пульт',palette:'graphite',description:'Светлая приборная панель: клавиши с фаской, углубления и ясные состояния.',material:'Аппаратные контролы'},
    {id:'tide-satin',name:'Прилив · Сатин',short:'Сатин',palette:'berry',description:'Матовые белые модули, мягкие скругления и спокойная цветовая иерархия.',material:'Мягкие модули'}
  ],
  palettes: [
    {id:'orbit',name:'Орбита',color:'#73539e'},
    {id:'tide',name:'Прилив',color:'#267a70'},
    {id:'atelier',name:'Ателье',color:'#9b5268'},
    {id:'grove',name:'Сад',color:'#47775a'},
    {id:'gallery',name:'Галерея',color:'#6d668e'},
    {id:'studio',name:'Студия',color:'#a85247'},
    {id:'pearl',name:'Жемчуг',color:'#087e89'},
    {id:'prism',name:'Призма',color:'#6151cd'},
    {id:'lagoon',name:'Лагуна',color:'#167896',isNew:true},
    {id:'denim',name:'Деним',color:'#315ca8',isNew:true},
    {id:'berry',name:'Ягодная',color:'#a3336a',isNew:true},
    {id:'slate',name:'Сланец',color:'#54707b',isNew:true},
    {id:'iris',name:'Ирис',color:'#7752b1',isNew:true},
    {id:'coral',name:'Коралл',color:'#ba5357',isNew:true},
    {id:'pine',name:'Хвоя',color:'#2a6a58',isNew:true},
    {id:'graphite',name:'Графит',color:'#515963',isNew:true}
  ]
};
