
$(function(){
  $('pre').hide();
  $('.view-source').click(function(e){
    $(this).next('pre').slideToggle('fast');
    e.preventDefault();
  });
})