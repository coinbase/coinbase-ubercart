<?php

require_once(dirname(__FILE__). "/coinbase-php/lib/Coinbase.php");

function uc_coinbase_success() {

  $order_id = $custom['order_id'];
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

function uc_coinbase_oauth_callback() {

  $clientId = variable_get('uc_coinbase_oauth_clientid');
  $clientSecret = variable_get('uc_coinbase_oauth_clientsecret');
  $redirectUrl = variable_get('uc_coinbase_oauth_last_redirect_url');
  $oauth = new Coinbase_OAuth($clientId, $clientSecret, $redirectUrl);
  
  $tokens = $oauth->getTokens($_GET['code']);
  variable_set('uc_coinbase_oauth_tokens', serialize($tokens));
  
  $url = url('admin/store/settings/payment/method/coinbase', array('absolute' => true));
  return "Success! Your merchant account has been connected and you're ready to accept bitcoin payments with Coinbase.<br><br><a href='$url'>Return to Coinbase settings</a>";
}

function uc_coinbase_callback() {

  // 1. Check secret
  $secret = variable_get('uc_coinbase_secret');
  if ($_GET['coinbase_secret'] !== $secret) {
    die("Malformed Coinbase callback (invalid URL)");
  }

  $tokens = unserialize(variable_get('uc_coinbase_oauth_tokens'));
  $clientId = variable_get('uc_coinbase_oauth_clientid');
  $clientSecret = variable_get('uc_coinbase_oauth_clientsecret');
  $oauth = new Coinbase_OAuth($clientId, $clientSecret, "");
  $coinbase = new Coinbase($oauth, $tokens);

  // 2. Download order data and verify
  $postBody = json_decode(file_get_contents("php://input"));
  if ($postBody == NULL) {
    die("Malformed Coinbase callback (no data supplied)");
  }

  $cb_order_id = $postBody->order->id;
  try {
    $order = $coinbase->getOrder($cb_order_id);
  } catch (Coinbase_TokensExpiredException $e) {
    // Refresh tokens
    $tokens = $oauth->refreshTokens($tokens);
    variable_set('uc_coinbase_oauth_tokens', serialize($tokens));
    $coinbase = new Coinbase($oauth, $tokens);
    $order = $coinbase->getOrder($cb_order_id);
  }
  $custom = unserialize(base64_decode($order->custom));
  if ($custom['secret'] !== sha1($secret)) {
    die("Malformed Coinbase callback (invalid order custom param)");
  }

  // Callback is legitimate - process order!
  $uc_order_id = $custom['order_id'];
  $uc_order = uc_order_load($uc_order_id);

  if ($order->status == "completed") {
    uc_payment_enter($uc_order->order_id, 'coinbase', ($order->total_native->cents / 100), 0, NULL, "Coinbase payment " . $cb_order_id);
    uc_cart_complete_sale($uc_order);
    uc_order_comment_save($uc_order_id, 0, t('Coinbase payment: @id of @amount @currency', array('@id' => $cb_order_id, '@amount' => uc_currency_format(($order->total_native->cents / 100), FALSE), '@currency' => $order->total_native->currency_iso)));
    echo "Payment logged for order $uc_order_id";
  } else {
    uc_order_comment_save($uc_order_id, 0, t('Coinbase order is now @status', array('@status' => $order->status)));
    echo "Incomplete status logged for order $uc_order_id";
  }
}
