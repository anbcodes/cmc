#!/bin/bash

# This is a custom executable that "pretends" to be a java runtime, but instead launches my app

POSITIONAL_ARGS=()

while [[ $# -gt 0 ]]; do
  case $1 in
    --accessToken)
      ACCESS_TOKEN="$2"
      shift # past argument
      shift # past value
      ;;
    --uuid)
      UUID="$2"
      shift # past argument
      shift # past value
      ;;
    --username)
      USERNAME="$2"
      shift # past argument
      shift # past value
      ;;
    *)
      POSITIONAL_ARGS+=("$1") # save positional arg
      shift # past argument
      ;;
  esac
done

set -- "${POSITIONAL_ARGS[@]}" # restore positional parameters

echo "username $USERNAME, uuid $UUID, access_token $ACCESS_TOKEN" > /tmp/args.txt

pushd $(dirname $0)

echo "./build/cmc $USERNAME 0.0.0.0 25565 $UUID $ACCESS_TOKEN" > ./run-with-token.sh

chmod +x ./run-with-token.sh

popd
