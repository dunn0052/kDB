import Connection

import argparse
import logging

def main():
    parser = argparse.ArgumentParser(description="Generate your vaccination QR code.")
    parser.add_argument("-p", "--port", type=int, help="Port to connect to", required=True)
    parser.add_argument("-a", "--address", type=str, help="Address to connect to", required=True)
    args = parser.parse_args()
    
    Connection.connectTo(args.address, args.port)
    

if __name__ == "__main__":
    try:
        main()
    except ValueError as e:
        logging.error(e)
    except Exception as e:
        logging.exception(e)