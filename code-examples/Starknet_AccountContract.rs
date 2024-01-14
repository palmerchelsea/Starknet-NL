use starknet::account::Call;
use starknet::get_caller_address;
use starknet::call_contract_syscall;
use starknet::get_tx_info;
use starknet::Span;
use starknet::felt::felt252;
use starknet::Array;

mod SUPPORTED_TX_VERSION {
    const DEPLOY_ACCOUNT: felt252 = 1;
    const DECLARE: felt252 = 2;
    const INVOKE: felt252 = 1;
}

const SIMULATE_TX_VERSION_OFFSET: felt252 = 340282366920938463463374607431768211456; // 2**128
const SRC6_TRAIT_ID: felt252 = 1270010605630597976495846281167968799381097569185364931397797212080166453709;

#[starknet::interface]
trait IAccount<T> {
    fn is_valid_signature(self: @T, hash: felt252, signature: Array<felt252>) -> felt252;
    fn supports_interface(self: @T, interface_id: felt252) -> bool;
    fn public_key(self: @T) -> felt252;
}

#[starknet::contract]
mod Account {
    use super::*;

    #[storage]
    struct Storage {
        public_key: felt252,
    }

    #[generate_trait]
    impl PrivateImpl of PrivateTrait {
        fn only_protocol(self: @ContractState) {
            let sender = get_caller_address();
            assert(sender.is_zero(), "Account: invalid caller");
        }

        fn validate_transaction(self: @ContractState) -> felt252 {
            let tx_info = get_tx_info().unbox();
            let tx_hash = tx_info.transaction_hash;
            let signature = tx_info.signature;

            let is_valid = self.is_valid_signature_bool(tx_hash, signature);
            assert(is_valid, "Account: Incorrect tx signature");
            'VALID'
        }

        fn only_supported_tx_version(self: @ContractState, supported_tx_version: felt252) {
            let tx_info = get_tx_info().unbox();
            let version = tx_info.version;
            assert(
                version == supported_tx_version || version == SIMULATE_TX_VERSION_OFFSET + supported_tx_version,
                "Account: Unsupported tx version",
            );
        }

        fn execute_single_call(self: @ContractState, call: Call) -> Span<felt252> {
            let Call { to, selector, calldata } = call;
            call_contract_syscall(to, selector, calldata.span()).unwrap_syscall()
        }

        fn execute_multiple_calls(self: @ContractState, mut calls: Array<Call>) -> Array<Span<felt252>> {
            let mut res = Array::new();
            loop {
                match calls.pop_front() {
                    Option::Some(call) => {
                        let _res = self.execute_single_call(call);
                        res.append(_res);
                    }
                    Option::None => {
                        break ();
                    }
                };
            }
            res
        }
    }

    #[external(v0)]
    #[generate_trait]
    impl ProtocolImpl of ProtocolTrait {
        fn __validate__(self: @ContractState, calls: Array<Call>) -> felt252 {
            self.only_protocol();
            self.only_supported_tx_version(SUPPORTED_TX_VERSION::INVOKE);
            self.validate_transaction()
        }

        fn __validate_declare__(self: @ContractState, class_hash: felt252) -> felt252 {
            self.only_protocol();
            self.only_supported_tx_version(SUPPORTED_TX_VERSION::DECLARE);
            self.validate_transaction()
        }

        fn __validate_deploy__(self: @ContractState, class_hash: felt252, salt: felt252, public_key: felt252) -> felt252 {
            self.only_protocol();
            self.only_supported_tx_version(SUPPORTED_TX_VERSION::DEPLOY_ACCOUNT);
            self.validate_transaction()
        }

        fn __execute__(ref self: ContractState, calls: Array<Call>) -> Array<Span<felt252>> {
            self.only_protocol();
            self.only_supported_tx_version(SUPPORTED_TX_VERSION::INVOKE);
            self.execute_multiple_calls(calls)
        }
    }

    #[external(v0)]
    #[generate_trait]
    impl AccountImpl of IAccount<ContractState> {
        fn is_valid_signature(self: @ContractState, hash: felt252, signature: Array<felt252>) -> felt252 { ... }

        fn supports_interface(self: @ContractState, interface_id: felt252) -> bool {
            interface_id == SRC6_TRAIT_ID
        }

        fn public_key(self: @ContractState) -> felt252 {
            self.public_key.read()
        }
    }
}
