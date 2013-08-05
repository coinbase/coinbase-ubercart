<?php

function uc_coinbase_success() {

  $order_id = $_GET['order']['custom'];
  if(!isset($_SESSION['cart_order']) || intval($_SESSION['cart_order']) != $order_id) {
    // Wrong order ID - something has gone wrong, but payment might have gone through
    drupal_set_message(t('Your payment has been processed.'));
    drupal_goto('cart');
    return;
  }

  $order = uc_order_load($_SESSION['cart_order']);
  if ($order->payment_method != 'coinbase') {
    drupal_goto('cart');
    return;
  }

  $_SESSION['uc_checkout'][$_SESSION['cart_order']]['do_complete'] = TRUE;
  drupal_goto('cart/checkout/complete');
}

function uc_coinbase_cancel() {
  unset($_SESSION['cart_order']);

  drupal_set_message(t('Your payment was cancelled.'));

  drupal_goto('cart');

  return 'test';
}

function uc_coinbase_callback() {

  // The callback cannot be trusted, so retreive order data directly from Coinbase
  $cb_order_id = $_GET['order']['id'];
  $coinbase = new Coinbase(variable_get('uc_coinbase_api_key'));
  $order = $coinbase->getOrder($cb_order_id);

  $uc_order_id = $order->custom;
  $uc_order = uc_order_load($uc_order_id);

  if($order->status == "completed") {
    uc_payment_enter($uc_order->order_id, 'coinbase', ($order->total_native->cents / 100), 0, NULL, "Coinbase payment " . $cb_order_id);
    uc_cart_complete_sale($uc_order);
    uc_order_comment_save($uc_order_id, 0, t('Coinbase payment: @amount @currency', array('@amount' => uc_currency_format(($order->total_native->cents / 100), FALSE), '@currency' => $order->total_native->currency_iso)));
  } else {
    echo 'Order not yet completed - ' . $order->status;
  }
}